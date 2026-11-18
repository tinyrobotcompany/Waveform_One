#!/usr/bin/env python3
"""Patch pinned ESP-Hosted sources into the build directory, never in-place."""
import argparse
import hashlib
from pathlib import Path

HASHES = {
    'sdio': '9868f97b7c68e5d8f1a3ac904dc6922b8ba0cc160c28c6a1402cd5c4147a652a',
    'mempool': 'f44d782f74a5f0518343d11dd50fdef127e899844268cb8d26a98f4ea5fe656b',
}


def replace_once(text, before, after):
    if text.count(before) != 1:
        raise ValueError('Pinned ESP-Hosted memory patch location changed')
    return text.replace(before, after, 1)


def patch_mempool(text):
    text = replace_once(text, '#include "mempool.h"',
                        '#include "mempool.h"\n#include "hosted_dma_memory.h"')
    text = replace_once(text, 'MEM_ALLOC(MEMPOOL_ALIGNED(mp->block_size))',
                        'waveform_hosted_buffer_alloc(MEMPOOL_ALIGNED(mp->block_size))')
    return replace_once(text, 'g_h.funcs->_h_malloc_align(MEMPOOL_ALIGNED(nbytes), MEMPOOL_ALIGNMENT_BYTES)',
                        'waveform_hosted_buffer_alloc(MEMPOOL_ALIGNED(nbytes))')


def patch_sdio(text):
    text = replace_once(text, '#include "string.h"',
                        '#include "string.h"\n#include "hosted_dma_memory.h"')
    text = replace_once(text, '\t\t\tassert(sendbuf);',
                        '\t\t\t/* Existing NULL path frees the queued payload and drops this TX. */')
    begin = text.index('\tif (len > double_buf.buffer[index].buf_size) {')
    end = text.index('\n\treturn *buf;', begin)
    text = text[:begin] + '''\treturn waveform_hosted_stream_buffer(buf, &double_buf.buffer[index].buf_size, len);''' + text[end + len('\n\treturn *buf;'):]
    before = '\t\tpkt_rxbuff = sdio_buffer_alloc(MEMSET_REQUIRED);\n\t\tassert(pkt_rxbuff);\n\n'
    text = replace_once(text, before, '')
    text = replace_once(text, '\t\tmemcpy(pkt_rxbuff, buf, packet_size);', '''\t\tpkt_rxbuff = sdio_buffer_alloc(MEMSET_REQUIRED);
\t\tif (!pkt_rxbuff) {
\t\t\tESP_LOGE(TAG, "RX memory unavailable; dropping the received stream");
\t\t\treturn ESP_ERR_NO_MEM;
\t\t}
\t\tmemcpy(pkt_rxbuff, buf, packet_size);''')
    text = replace_once(text, '\t\trxbuff = sdio_rx_get_buffer(len_from_slave);\n\t\tassert(rxbuff);', '''\t\trxbuff = sdio_rx_get_buffer(len_from_slave);
\t\twhile (!rxbuff) {
\t\t\t/* No bytes have been read: preserve the pending length/counter.
\t\t\t * Let TX and consumers release memory instead of asserting or
\t\t\t * waiting for a fresh interrupt after this one was acknowledged. */
\t\t\tSDIO_DRV_UNLOCK();
\t\t\tvTaskDelay(pdMS_TO_TICKS(10));
\t\t\tSDIO_DRV_LOCK();
\t\t\trxbuff = sdio_rx_get_buffer(len_from_slave);
\t\t}''')
    return replace_once(text, '\t\t\t// don\'t send data to task, or update write_index',
                        '\t\t\tsdio_rx_free_buffer(rxbuff);\n\t\t\t// don\'t send data to task, or update write_index')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kind', choices=HASHES)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    data = args.source.read_bytes()
    if hashlib.sha256(data).hexdigest() != HASHES[args.kind]:
        raise ValueError('ESP-Hosted source changed; review the memory patch')
    transform = patch_sdio if args.kind == 'sdio' else patch_mempool
    args.output.write_text(transform(data.decode()))


if __name__ == '__main__':
    main()
