#ifndef __G0M_PDP0_HW_H
#define __G0M_PDP0_HW_H
#include "pdp0_hw.h"

void g0m_pdp0_hw_init(struct innodpu_pdp0_hw_device *hwdev, int dpu_id);
bool g0m_pdp_filter(int dpu_id);
#endif
