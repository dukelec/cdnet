/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "cd_utils.h"
#include "cdctl_pll_cal.h"


pllcfg_t cdctl_pll_cal(uint32_t input, uint32_t output) {
    pllcfg_t best = {0, 0, 0, 0xffffffff, 0xffffffff};
    uint32_t min_vco = 100e6L, max_vco = 500e6L, target_vco = 300e6L;
    uint32_t min_div_freq = 1e6L, max_div_freq = 15e6L, target_div_freq = 8e6L;

    for (int d = 0; d <= 2; d++) {
        uint32_t factor_d = 1 << d; // pow(2, d)

        for (int n = 31; n >= 0; n--) {
            uint32_t div_freq = DIV_ROUND_CLOSEST(input, n + 2);
            if (div_freq < min_div_freq)
                continue;
            if (div_freq > max_div_freq)
                break;

            for (int m = 0; m < 512; m++) {
                uint32_t vco_freq = div_freq * (m + 2);
                if (vco_freq < min_vco)
                    continue;
                if (vco_freq > max_vco)
                    break;

                uint32_t computed_output = DIV_ROUND_CLOSEST(vco_freq, factor_d);
                uint32_t error = abs(computed_output - output);

                // optimize div_freq and vco_freq
                uint32_t div_freq_deviation = abs(div_freq - target_div_freq);
                uint32_t vco_freq_deviation = abs(vco_freq - target_vco);
                uint32_t total_deviation = div_freq_deviation + vco_freq_deviation;

                if (error < best.error || (error == best.error && total_deviation < best.deviation)) {
                    best.n = n;
                    best.m = m;
                    best.d = d;
                    best.error = error;
                    best.deviation = total_deviation;
                }
            }
        }
    }

    if (best.d == 2)
        best.d = 3;
    return best;
}


uint32_t cdctl_pll_get(uint32_t input, pllcfg_t cfg)
{
    if (cfg.d == 3)
        cfg.d = 2;
    uint32_t div_freq = DIV_ROUND_CLOSEST(input, cfg.n + 2);
    uint32_t vco_freq = div_freq * (cfg.m + 2);
    return vco_freq / (1 << cfg.d);
}


uint32_t cdctl_sys_cal(uint32_t baud) {
    uint32_t best[2] = {0, 0xffffffff};
    uint32_t clk_max = 150e6L;
    uint32_t clk_min = 100e6L; // higher sysclk for higher spi clk
    uint32_t clk_step = 2e5L;

    for (uint32_t c = clk_max; c >= clk_min; c -= clk_step) {
        uint32_t div = DIV_ROUND_CLOSEST(c, baud);
        uint32_t error = abs(DIV_ROUND_CLOSEST(c, div) - baud);

        if (error < best[1]) {
            best[0] = c;
            best[1] = error;
            if (error == 0)
                break;
        }
    }

    return best[0];
}
