#pragma once

#include "../../common.h"
#include <memory>
// used: draw system
#include "../../utilities/draw.h"
#include "../../sdk/datatypes/vector.h"
#include "../../sdk/datatypes/transform.h"
#include "../../sdk/datatypes/qangle.h"
#include "../cstrike/core/config.h"
class CCSPlayerController;
class C_BaseEntity;
class C_CSPlayerPawn;
class CBaseHandle;
class CEntityInstance;
class CUserCmd;
class CBaseUserCmdPB;
class QAngle_t;
class CCSGOInput;
class IGameEvent;
class C_CSWeaponBase;
class CCSWeaponBaseVData;
class legit_scan_data;
class legit_scan_point;
class legit_scanned_target;
#define MAX_STUDIO_BONES 1024

#include <array>

namespace F::LEGIT
{


	class impl {
	public:


		enum reset {
			entity,
			aimbot

		};
		enum scan_mode {
			single, /* only 1 hitbox */
			adaptive /* multiple bones pushed in the same time */
		};

		enum hitboxes {
			scan_head,
			scan_neck,
			scan_chest,
			scan_pelvis
		};
		enum events {
			round_start = 1,
			player_death = 2,
		};

		enum commands_msg {
			none = 0,
			viewangles = 1,

		};
		enum responses_msg {
			empty = 0,
			validated_view_angle = 1,
		};

		struct CSubTickData {
			QAngle_t best_point = QAngle_t(0, 0, 0);
			Vector_t best_point_vec = Vector_t(0, 0, 0);
			commands_msg command = commands_msg::none;
			responses_msg response = responses_msg::empty;

			void reset() {
				best_point.Reset();
				best_point_vec = Vector_t(0, 0, 0);
				command = commands_msg::none;
				response = responses_msg::empty;
			}

		}sub_tickdata;


		struct aim_info {
			bool legit_enable;
			int smooth;
			float rcs_x;
			float rcs_y;
			bool rcs_control;
			bool smoothing;
			bool triggerbot;
			bool key_enabled;
			bool hitbox_head;
			bool hitbox_stomach;
			bool hitbox_legs;
			bool hitbox_feets;
			bool hitbox_chest;
			float fov;
			bool perfect_silent;
		}legit_data;

		float GetSpread(C_CSWeaponBase* weapon);
		float GetInaccuracy(C_CSPlayerPawn* pLocal, C_CSWeaponBase* weapon);
		void SortTarget(QAngle_t viewangles);
		/* void inits */
		QAngle_t PerformSmooth(QAngle_t currentAngles, const QAngle_t& targetAngles, float smoothFactor, C_CSPlayerPawn* pLocal, Vector_t hitboxPos);
		bool valid(C_CSPlayerPawn* pawn, C_CSPlayerPawn* pLocal, bool check = false);
		void ScanPoint(C_CSPlayerPawn* pLocal, CUserCmd* cmd, C_CSPlayerPawn* record, legit_scan_data& data, const Vector_t& shoot_pos, bool optimized);
		void Scan();
		void Triggerbot(CUserCmd* cmd, C_BaseEntity* localent, C_BaseEntity* playerent, C_CSPlayerPawn* local, C_CSPlayerPawn* player, CCSWeaponBaseVData* vdata);
		void BuildSeed();
		void SetupTarget(C_CSPlayerPawn* pawn);
		void SetupAdaptiveWeapon(C_CSPlayerPawn* pawn);
		void ScanTarget(C_CSPlayerPawn* pLocal, CUserCmd* cmd, QAngle_t viewangles);
		Vector_t eye_pos;
		float ScaleDamage(C_CSPlayerPawn* target, C_CSPlayerPawn* pLocal, C_CSWeaponBase* weapon, Vector_t aim_point, float& dmg, bool& canHit);
		bool CanHit(Vector_t start, Vector_t end, C_CSPlayerPawn* pLocal, C_CSPlayerPawn* record, int box);
		void Reset(reset type);
		void Run(C_CSPlayerPawn* pLocal, CCSGOInput* pInput, CUserCmd* cmd);
		bool Ready(C_CSPlayerPawn* pLocal);
		void Events(IGameEvent* event_listener, events type);
		/* init class */
		aim_info aimbot_info;
		inline static std::vector<std::pair<float, float>> m_computed_seeds;

	};
	const auto legit = std::make_unique<impl>();
}

