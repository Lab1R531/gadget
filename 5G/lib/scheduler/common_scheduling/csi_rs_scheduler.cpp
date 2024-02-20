/*
 *
 * Copyright 2021-2023 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include "csi_rs_scheduler.h"
#include "srsran/ran/csi_rs/csi_rs_config_helpers.h"
#include "srsran/koffset.h"

using namespace srsran;

static csi_rs_info build_csi_rs_info(const bwp_configuration& bwp_cfg, const nzp_csi_rs_resource& nzp_csi_rs_res)
{
  csi_rs_info csi_rs;

  csi_rs.bwp_cfg = &bwp_cfg;
  csi_rs.crbs    = {nzp_csi_rs_res.res_mapping.freq_band_start_rb,
                    nzp_csi_rs_res.res_mapping.freq_band_start_rb + nzp_csi_rs_res.res_mapping.freq_band_nof_rb};
  csi_rs.type    = srsran::csi_rs_type::CSI_RS_NZP;

  csi_rs.freq_domain = nzp_csi_rs_res.res_mapping.fd_alloc;
  csi_rs.row         = csi_rs::get_csi_rs_resource_mapping_row_number(nzp_csi_rs_res.res_mapping.nof_ports,
                                                              nzp_csi_rs_res.res_mapping.freq_density,
                                                              nzp_csi_rs_res.res_mapping.cdm,
                                                              nzp_csi_rs_res.res_mapping.fd_alloc);
  srsran_assert(csi_rs.row > 0, "The CSI-RS configuration resulted in an invalid row of Table 7.4.1.5.3-1, TS 38.211");

  csi_rs.symbol0                      = nzp_csi_rs_res.res_mapping.first_ofdm_symbol_in_td;
  csi_rs.symbol1                      = nzp_csi_rs_res.res_mapping.first_ofdm_symbol_in_td2.has_value()
                                            ? *nzp_csi_rs_res.res_mapping.first_ofdm_symbol_in_td2
                                            : 2;
  csi_rs.cdm_type                     = nzp_csi_rs_res.res_mapping.cdm;
  csi_rs.freq_density                 = nzp_csi_rs_res.res_mapping.freq_density;
  csi_rs.scrambling_id                = nzp_csi_rs_res.scrambling_id;
  csi_rs.power_ctrl_offset_profile_nr = nzp_csi_rs_res.pwr_ctrl_offset;
  csi_rs.power_ctrl_offset_ss_profile_nr =
      nzp_csi_rs_res.pwr_ctrl_offset_ss_db.has_value() ? *nzp_csi_rs_res.pwr_ctrl_offset_ss_db : 0;

  return csi_rs;
}

csi_rs_scheduler::csi_rs_scheduler(const cell_configuration& cell_cfg_) : cell_cfg(cell_cfg_)
{
  if (cell_cfg.csi_meas_cfg.has_value()) {
    for (const auto& nzp_csi : cell_cfg.csi_meas_cfg->nzp_csi_rs_res_list) {
      cached_csi_rs.push_back(build_csi_rs_info(cell_cfg.dl_cfg_common.init_dl_bwp.generic_params, nzp_csi));
    }
  }
}

void csi_rs_scheduler::run_slot(cell_slot_resource_allocator& res_grid)
{
  if (cached_csi_rs.empty()) {
    return;
  }
  if (not cell_cfg.is_fully_dl_enabled(res_grid.slot)) {
    return;
  }

  for (unsigned i = 0; i != cached_csi_rs.size(); ++i) {
    const nzp_csi_rs_resource& nzp_csi = cell_cfg.csi_meas_cfg->nzp_csi_rs_res_list[i];
    /* DCD account for Koffset and avoid negative modulo errors */
    uint32_t slot_idx = res_grid.slot.to_uint();
    uint32_t adjust = *nzp_csi.csi_res_offset + NTN_KOFFSET;
    if (slot_idx >= adjust && (slot_idx - adjust) % (unsigned)*nzp_csi.csi_res_period == 0)
      res_grid.result.dl.csi_rs.push_back(cached_csi_rs[i]);
  }
}
