#include "SlippiExiTypes.h"
#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"

template <typename T> static T SlippiExiTypes::Convert(u8 *payload)
{
	return *reinterpret_cast<T *>(payload);
}

// Here we define custom convert functions for any type that larger than u8 sized fields to convert from big-endian

template <> static SlippiExiTypes::ReportGameQuery SlippiExiTypes::Convert(u8 *payload)
{
	auto q = *reinterpret_cast<SlippiExiTypes::ReportGameQuery *>(payload);
	q.frameLength = Common::FromBigEndian(q.frameLength);
	q.gameIndex = Common::FromBigEndian(q.gameIndex);
	q.tiebreakIndex = Common::FromBigEndian(q.tiebreakIndex);
	for (int i = 0; i < REPORT_PLAYER_COUNT; i++)
	{
		auto *p = &q.players[i];
		p->damageDone = Common::FromBigEndian(p->damageDone);
	}
	return q;
}

template <> static SlippiExiTypes::OverwriteSelectionsQuery SlippiExiTypes::Convert(u8 *payload)
{
	auto q = *reinterpret_cast<SlippiExiTypes::OverwriteSelectionsQuery *>(payload);
	q.stage_id = Common::FromBigEndian(q.stage_id);
	return q;
}

template <> static SlippiExiTypes::GpFetchStepQuery SlippiExiTypes::Convert(u8 *payload)
{
	return *reinterpret_cast<SlippiExiTypes::GpFetchStepQuery *>(payload);
}

template <> static SlippiExiTypes::GpCompleteStepQuery SlippiExiTypes::Convert(u8 *payload)
{
	return *reinterpret_cast<SlippiExiTypes::GpCompleteStepQuery *>(payload);
}