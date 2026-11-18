import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'scripts/patch-hosted-memory.py'
spec = importlib.util.spec_from_file_location('memory_patch', SCRIPT)
patch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patch)

# Exact source anchors from the pinned driver. The actual complete patched driver
# is compiled by the P4 build; these tests exercise retry and malformed-source paths.
FIXTURE = '''#include "string.h"
\t\t\tassert(sendbuf);
\tif (len > double_buf.buffer[index].buf_size) {
        old_growth();
    }
\treturn *buf;
\t\tpkt_rxbuff = sdio_buffer_alloc(MEMSET_REQUIRED);
\t\tassert(pkt_rxbuff);

    validate_packet_size();
\t\tmemcpy(pkt_rxbuff, buf, packet_size);
\t\trxbuff = sdio_rx_get_buffer(len_from_slave);
\t\tassert(rxbuff);
END_RETRY
\t\t\t// don't send data to task, or update write_index
'''


class HostedMemoryPatchTests(unittest.TestCase):
    def test_retry_releases_bus_and_keeps_pending_length(self):
        patched = patch.patch_sdio(FIXTURE)
        retry = patched[patched.index('\t\trxbuff ='):patched.index('END_RETRY')]
        program = '''#include <assert.h>
#include <stddef.h>
static int locked=1, attempts, waits;
static unsigned char buffer[32];
static void *sdio_rx_get_buffer(unsigned len) {
 assert(locked && len==32); return ++attempts==3 ? buffer : NULL;
}
#define SDIO_DRV_UNLOCK() do { assert(locked); locked=0; } while(0)
#define SDIO_DRV_LOCK() do { assert(!locked); locked=1; } while(0)
#define pdMS_TO_TICKS(n) (n)
static void vTaskDelay(unsigned n) { assert(!locked && n==10); ++waits; }
int main(void) { void *rxbuff; unsigned len_from_slave=32;
''' + retry + '''
assert(rxbuff==buffer && attempts==3 && waits==2 && locked);
}
'''
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / 'retry.c'
            binary = Path(temp) / 'retry'
            source.write_text(program)
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', str(source), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
        self.assertNotIn('assert(sendbuf)', patched)
        self.assertNotIn('assert(pkt_rxbuff)', patched)
        self.assertLess(patched.index('validate_packet_size()'), patched.index('pkt_rxbuff = sdio_buffer_alloc'))
        self.assertIn('return ESP_ERR_NO_MEM', patched)
        self.assertIn('sdio_rx_free_buffer(rxbuff)', patched)

    def test_pool_paths_both_use_psram_allocator(self):
        source = '#include "mempool.h"\nMEM_ALLOC(MEMPOOL_ALIGNED(mp->block_size));\ng_h.funcs->_h_malloc_align(MEMPOOL_ALIGNED(nbytes), MEMPOOL_ALIGNMENT_BYTES);'
        result = patch.patch_mempool(source)
        self.assertEqual(result.count('waveform_hosted_buffer_alloc('), 2)
        self.assertNotIn('MEM_ALLOC(', result)

    def test_missing_or_duplicate_patch_locations_fail(self):
        for source in [FIXTURE.replace('assert(rxbuff)', 'other(rxbuff)'), FIXTURE + '\n\t\t\tassert(sendbuf);']:
            with self.assertRaises(ValueError):
                patch.patch_sdio(source)

    def test_cli_rejects_changed_vendor_source_without_output(self):
        with tempfile.TemporaryDirectory() as temp:
            source, output = Path(temp) / 'source.c', Path(temp) / 'output.c'
            source.write_text(FIXTURE)
            result = subprocess.run(['python3', str(SCRIPT), 'sdio', str(source), str(output)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(output.exists())
