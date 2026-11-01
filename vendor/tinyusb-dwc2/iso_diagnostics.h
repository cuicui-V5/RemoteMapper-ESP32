#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 计数从设备启动开始累计，读取不会改变计数。
uint32_t remotemapper_dwc2_iso_incomplete_count(void);
uint32_t remotemapper_dwc2_iso_retry_count(void);
uint32_t remotemapper_dwc2_iso_failed_count(void);

typedef struct {
    uint32_t daint;
    uint32_t daintmsk;
    uint32_t diepempmsk;
    uint32_t ctl[6];
    uint32_t interrupt[6];
    uint32_t transfer_size[6];
    uint16_t max_size[6];
    uint8_t endpoint_count;
} remotemapper_dwc2_diagnostics_t;

void remotemapper_dwc2_diagnostics(remotemapper_dwc2_diagnostics_t* output);

#ifdef __cplusplus
}
#endif
