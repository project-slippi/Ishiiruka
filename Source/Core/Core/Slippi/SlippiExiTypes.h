#pragma once

#include "Common/CommonTypes.h"

class SlippiExiTypes
{
  public:
// Using pragma pack here will remove any structure padding which is what EXI comms expect
// https://www.geeksforgeeks.org/how-to-avoid-structure-padding-in-c/
#pragma pack(1)

	struct GpCompleteStepQuery
	{
		u8 command;
		u8 step_idx;
		u8 char_selection;
		u8 char_color_selection;
		u8 stage_selections[2];
	};

	struct GpFetchStepQuery
	{
		u8 command;
		u8 step_idx;
	};

	struct GpFetchStepResponse
	{
		u8 is_found;
		u8 is_skip;
		u8 char_selection;
		u8 char_color_selection;
		u8 stage_selections[2];
	};

	struct OverwriteCharSelections
	{
		u8 is_set;
		u8 char_id;
		u8 char_color_id;
	};
	struct OverwriteSelectionsQuery
	{
		u8 command;
		u16 stage_id;
		OverwriteCharSelections chars[4];
	};

// Not sure if resetting is strictly needed, might be contained to the file
#pragma pack()
};