#pragma once

#include "Common/CommonTypes.h"

class SlippiExiTypes
{
  public:
	struct PrepCompleteStepQuery
	{
		u8 step_idx;
		u8 char_selection;
		u8 char_color_selection;
		u8 stage_selections[2];
	};
};