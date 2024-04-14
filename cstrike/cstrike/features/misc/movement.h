#pragma once
#pragma once

#include "../../common.h"
#include <memory>
// used: draw system
#include "../../utilities/draw.h"
#include "../../sdk/datatypes/vector.h"
#include "../../sdk/datatypes/transform.h"
#include "../../sdk/datatypes/qangle.h"
#include "../cstrike/core/config.h"

class CUserCmd;
class CBaseUserCmdPB;
class CCSGOInputHistoryEntryPB;
class CBasePlayerController;
class CCSPlayerController;
class C_CSPlayerPawn;
class CSubtickMoveStep;
class QAngle_t;

namespace F::MISC::MOVEMENT
{
	class impl {
	public:

		struct MovementData {

			QAngle_t angCorrectionView = QAngle_t(0, 0, 0);
			QAngle_t angStoredView = QAngle_t(0, 0, 0);

			void reset() {
				angCorrectionView.Reset();
				angStoredView.Reset();
			}

		}_move_data;

		void EdgeBug(CCSPlayerController* controler, C_CSPlayerPawn* localPlayer, CUserCmd* cmd);

		void movment_fix(CUserCmd* pCmd, QAngle_t angle);

		void ProcessMovement(CUserCmd* pCmd, CCSPlayerController* pLocalController, C_CSPlayerPawn* pLocalPawn);

		void BunnyHop(CUserCmd* pCmd, CBaseUserCmdPB* pUserCmd, C_CSPlayerPawn* pLocalPawn);
		void AutoStrafe(CUserCmd* pCmd, CBaseUserCmdPB* pUserCmd, C_CSPlayerPawn* pLocalPawn, int type);

		void MovementCorrection(CBaseUserCmdPB* pUserCmd, CCSGOInputHistoryEntryPB* pInputHistory, const QAngle_t& angDesiredViewPoint);

		// will call MovementCorrection && validate user's angView to avoid untrusted ban
		void ValidateUserCommand(CUserCmd* pCmd, CBaseUserCmdPB* pUserCmd, CCSGOInputHistoryEntryPB* pInputHistory);
	};
	const auto movement = std::make_unique<impl>();

	
}
