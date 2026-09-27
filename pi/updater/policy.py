"""Keep customer update checks separate from development-device deployment."""


def mode(config):
    value = config.get('deployment_mode', 'production')
    if value not in ('production', 'development'):
        raise ValueError('Unknown device deployment mode')
    return value


def automatic_install(config, action):
    selected = mode(config)
    if action == 'sync-main' and selected != 'development':
        raise ValueError('Automatic main deployment is disabled on production devices')
    return action == 'sync-main'


def timer(selected):
    mode({'deployment_mode': selected})
    schedule = ('OnBootSec=30s\nOnUnitActiveSec=1min\nUnit=waveform-update-sync-main.service\n'
                if selected == 'development' else
                'OnCalendar=monthly\nRandomizedDelaySec=6h\nPersistent=true\nUnit=waveform-update-check.service\n')
    return ('[Unit]\nDescription=Waveform One update policy\n[Timer]\n' + schedule +
            '[Install]\nWantedBy=timers.target\n')
