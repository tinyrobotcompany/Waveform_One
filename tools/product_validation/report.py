"""Offline evidence reporting. Never performs recognition, changes firmware or invents results."""
import argparse
from collections import Counter, defaultdict
import csv
import hashlib
import json
import math
from pathlib import Path
import re

TYPES = {'acquisition', 'transition', 'mid_song', 'repeat', 'silence', 'non_music'}
OUTCOMES = {'correct', 'wrong', 'no_match', 'service_error'}
NUMBERS = ('first_correct_ms','stable_correct_ms','display_clear_ms','led_settle_ms',
           'music_duration_s','observation_duration_s','request_count','provider_billed_units')


def number(value):
    if value == '': return None
    result = float(value)
    if not math.isfinite(result) or result < 0: raise ValueError('Expected finite non-negative measurement')
    return result


def load_trials(path):
    with Path(path).open(newline='') as source:
        reader = csv.DictReader(source)
        required = {'run_id','trial_id','trial_type','condition_id','corpus_recording_id','outcome','any_wrong_answer',*NUMBERS}
        if not required.issubset(reader.fieldnames or []): raise ValueError('Missing protocol columns')
        rows = []
        seen = set()
        for raw in reader:
            identity = (raw['run_id'],raw['trial_id'])
            if not all(identity) or identity in seen: raise ValueError('Missing or duplicate trial ID')
            seen.add(identity)
            if raw['trial_type'] not in TYPES or raw['outcome'] not in OUTCOMES: raise ValueError('Invalid trial type/outcome')
            if not raw['condition_id']: raise ValueError('Missing condition ID')
            if raw['any_wrong_answer'].lower() not in ('true','false','1','0'): raise ValueError('Explicit wrong-answer flag required')
            row = dict(raw)
            for key in NUMBERS: row[key] = number(raw[key])
            for key in ('request_count','provider_billed_units'):
                if row[key] is not None and row[key] != int(row[key]): raise ValueError('Counts must be integers')
            if row['request_count'] is None or row['observation_duration_s'] is None: raise ValueError('Request count and observation duration required')
            row['any_wrong_answer'] = raw['any_wrong_answer'].lower() in ('true','1')
            if row['outcome']=='wrong' and not row['any_wrong_answer']: raise ValueError('Wrong outcome requires wrong-answer flag')
            rows.append(row)
    if not rows: raise ValueError('No trials: report not benchmarked; do not generate percentages')
    if len({r['run_id'] for r in rows}) != 1: raise ValueError('Do not mix runs')
    return rows


def percentile(values, p):
    if not values: return None
    values = sorted(values); position = (len(values)-1)*p
    lo, hi = math.floor(position), math.ceil(position)
    return values[lo] + (values[hi]-values[lo])*(position-lo)


def wilson(success, total):
    if not total: return None
    z=1.959963984540054; proportion=success/total; denominator=1+z*z/total
    center=(proportion+z*z/(2*total))/denominator
    margin=z*math.sqrt(proportion*(1-proportion)/total+z*z/(4*total*total))/denominator
    return [max(0,center-margin),min(1,center+margin)]


def summarize(rows):
    n=len(rows); outcomes=Counter(r['outcome'] for r in rows)
    distinct = all(r['corpus_recording_id'] for r in rows) and len({r['corpus_recording_id'] for r in rows})==n
    answer=outcomes['correct']+outcomes['wrong']
    result={'trials':n,'outcomes':dict(outcomes), 'accuracy':outcomes['correct']/n,
            'answer_precision':outcomes['correct']/answer if answer else None,
            'transient_wrong_rate':sum(r['any_wrong_answer'] for r in rows)/n,
            'accuracy_wilson95':wilson(outcomes['correct'],n) if distinct else None,
            'interval_note':'Distinct recordings only; no pooled-condition or repeat independence assumed.',
            'latency_ms':{}}
    for field in ('first_correct_ms','stable_correct_ms','display_clear_ms','led_settle_ms'):
        values=[r[field] for r in rows if r[field] is not None]
        result['latency_ms'][field]={'samples':len(values),'missing_or_timeout':n-len(values),'p50':percentile(values,.5),'p95':percentile(values,.95)}
    requests=sum(r['request_count'] for r in rows)
    music=sum(r['music_duration_s'] or 0 for r in rows)
    elapsed=sum(r['observation_duration_s'] for r in rows)
    result['usage']={'requests':requests,'requests_per_music_hour':requests*3600/music if music and all(r['music_duration_s'] is not None for r in rows) else None,
                     'requests_per_elapsed_hour':requests*3600/elapsed if elapsed else None,
                     'requests_per_correct_trial':requests/outcomes['correct'] if outcomes['correct'] else None,
                     'provider_billed_units':sum(r['provider_billed_units'] for r in rows) if all(r['provider_billed_units'] is not None for r in rows) else None}
    return result


def report(path, metadata):
    for field, size in [('firmware_sha',40),('image_sha256',64)]:
        if not re.fullmatch('[0-9a-f]{'+str(size)+'}', metadata.get(field,'')): raise ValueError('Exact firmware/image hashes required')
    for field in ('corpus_version','protocol_version','tested_utc','hardware','acoustic_setup','provider','evidence_path'):
        if not isinstance(metadata.get(field),str) or not metadata[field].strip(): raise ValueError('Missing run metadata: '+field)
    rows=load_trials(path); groups=defaultdict(list)
    for row in rows: groups[(row['trial_type'],row['condition_id'])].append(row)
    summaries={kind+'/'+condition:summarize(values) for (kind,condition),values in sorted(groups.items())}
    for key, value in summaries.items():
        if key.startswith(('silence/','non_music/')):
            value.pop('accuracy');value.pop('accuracy_wilson95');value.pop('answer_precision')
        if key.startswith('non_music/'):
            values=groups[tuple(key.split('/',1))]
            value['false_positive_trial_rate']=sum(r['any_wrong_answer'] for r in values)/len(values)
            value['false_positive_events_per_hour']=None # Trial flag is not an event count.
    return {'schema':1,'run_id':rows[0]['run_id'],'tested_firmware_sha':metadata['firmware_sha'],
            'input_sha256':hashlib.sha256(Path(path).read_bytes()).hexdigest(),'metadata':metadata,'groups':summaries,
            'limitations':['No pooled confidence interval: recordings repeat across conditions.',
                          'Latency quantiles use linear interpolation and successful observed times only.',
                          'Album/edition accuracy requires separate independently labelled evidence; not inferred.']}


def cost(plan):
    keys=('monthly_fee','included_units','overage_per_1000','devices','hours_per_day','days','requests_per_hour')
    for key in keys:
        if isinstance(plan.get(key),bool) or not isinstance(plan.get(key),(int,float)) or not math.isfinite(plan[key]) or plan[key]<0: raise ValueError('Invalid cost input: '+key)
    if plan['devices']<1 or plan['devices']!=int(plan['devices']):raise ValueError('Positive integer device count required')
    if plan.get('usage_basis') not in ('measured','configured_ceiling','assumption'):raise ValueError('Declare usage basis')
    if not isinstance(plan.get('currency'),str) or not plan['currency']:raise ValueError('Currency required')
    requests=plan['devices']*plan['hours_per_day']*plan['days']*plan['requests_per_hour']
    total=plan['monthly_fee']+max(0,requests-plan['included_units'])*plan['overage_per_1000']/1000
    return {'scenario_only':True,'terms_verified':plan.get('terms_verified') is True,'currency':plan['currency'],
            'usage_basis':plan['usage_basis'],'estimated_units':requests,'plan_cost':total,
            'cost_per_device':total/plan['devices'],'effective_cost_per_unit':total/requests if requests else None,
            'limitation':'Only a shared monthly allowance plus flat overage model; verify billing units, tax and OEM rights separately.'}


def main():
    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='command',required=True)
    benchmark=sub.add_parser('benchmark');benchmark.add_argument('trials',type=Path);benchmark.add_argument('metadata',type=Path)
    economics=sub.add_parser('cost');economics.add_argument('plan',type=Path)
    args=parser.parse_args()
    value=report(args.trials,json.loads(args.metadata.read_text())) if args.command=='benchmark' else cost(json.loads(args.plan.read_text()))
    print(json.dumps(value,indent=2,allow_nan=False))
if __name__=='__main__':main()
