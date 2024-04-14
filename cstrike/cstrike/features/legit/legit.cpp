#include "legit.h"
#include "../../core/config.h"
#include "../../core/variables.h"
#include "../../sdk/datatypes/usercmd.h"
#include "../../core/sdk.h"
#include "../../sdk/entity.h"
#include "../../sdk/interfaces/iengineclient.h"
#include "../../sdk/interfaces/iglobalvars.h"
#include "../../sdk/interfaces/cgameentitysystem.h"
#include "../../sdk/datatypes/qangle.h"
#include "../../sdk/datatypes/vector.h"
#include "../misc/movement.h"
#include "../cstrike/sdk/interfaces/ccsgoinput.h"
#include "../cstrike/sdk/interfaces/ienginecvar.h"
#include "../cstrike/sdk/interfaces/events.h"
#include "../penetration/penetration.h"
#include "../cstrike/sdk/interfaces/itrace.h"
#include "../cstrike/core/spoofcall/syscall.h"
#include <iostream>
#include <memoryapi.h>
#include <mutex>
#include <array>
#include "../../core/spoofcall/virtualization/VirtualizerSDK64.h"
#include "../../utilities/inputsystem.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "../../core/hooks.h"
#define M_PI_F ((float)(M_PI)) 
#ifndef RAD2DEG
#define RAD2DEG(x) ((float)(x) * (float)(180.f / M_PI_F))
#endif

#ifndef DEG2RAD
#define DEG2RAD(x) ((float)(x) * (float)(M_PI_F / 180.f))
#endif
static std::vector <std::uint32_t> e_hitboxes;
static bool m_stop = false;
static bool early_stopped = false;

#include <mutex>
#define MAX_STUDIO_BONES 1024

class legit_target
{
public:
    C_CSPlayerPawn* handle;
    C_BaseEntity* entHandle;
    legit_target()
    {
        handle = nullptr;
        entHandle = nullptr;
    }

    legit_target(C_CSPlayerPawn* handle, C_BaseEntity* entHandle)
    {
        this->handle = handle;
        this->entHandle = entHandle;
    }
};

class legit_scan_point
{
public:
    Vector_t point;
    Vector_t pointangle;
    QAngle_t aimangle;
    QAngle_t angle;

    int hitbox;
    bool center;
    float safe;
    float damage;
    bool canHit;

    legit_scan_point()
    {
        point = Vector_t(0, 0, 0);
        pointangle = Vector_t(0, 0, 0);
        aimangle = QAngle_t(0, 0, 0);
        angle = QAngle_t(0, 0, 0);
        hitbox = -1;
        center = false;
        safe = 0.0f;
        damage = 0.f;
        canHit = false;
    }

    legit_scan_point(const Vector_t& point, const Vector_t& pointangle, const QAngle_t& aimangle, const QAngle_t& angle, const int& hitbox, const bool& center, const float& damage, const bool& canHit) //-V818 //-V730
    {
        this->point = point;
        this->pointangle = pointangle;
        this->aimangle = aimangle;
        this->angle = angle;
        this->hitbox = hitbox;
        this->center = center;
        this->damage = damage;
        this->canHit = canHit;
    }

    void reset()
    {
        point = Vector_t(0, 0, 0);
        pointangle = Vector_t(0, 0, 0);
        aimangle = QAngle_t(0, 0, 0);
        angle = QAngle_t(0, 0, 0);
        hitbox = -1;
        center = false;
        safe = 0.0f;
        damage = 0.f;
        canHit = false;
    }
};
class legit_scan_data
{
public:
    legit_scan_point point;

    bool visible;
    int damage;
    int hitbox;

    legit_scan_data()
    {
        reset();
    }

    void reset()
    {
        point.reset();
        visible = false;
        damage = -1;
        hitbox = -1;
    }

    bool valid()
    {
        return damage >= 1 && point.angle.IsValid() && visible;
    }
};

class legit_scanned_target
{
public:
    C_CSPlayerPawn* record;
    C_BaseEntity* ent;
    legit_scan_data data;

    float fov;
    float distance;
    int health;
    float damage;
    legit_scanned_target()
    {
        reset();
    }

    legit_scanned_target(const legit_scanned_target& data) //-V688
    {
        this->record = data.record;
        this->data = data.data;
        this->fov = data.fov;
        this->distance = data.distance;
        this->health = data.health;
        this->damage = data.damage;
        this->ent = data.ent;

    }

    legit_scanned_target& operator=(const legit_scanned_target& data) //-V688
    {
        this->ent = data.ent;
        this->record = data.record;
        this->data = data.data;
        this->fov = data.fov;
        this->distance = data.distance;
        this->health = data.health;
        this->damage = data.damage;

        return *this;
    }

    legit_scanned_target(C_BaseEntity* ent, QAngle_t viewangles, float damage, float health, Vector_t eyepos, Vector_t end_pos, C_CSPlayerPawn* record, const legit_scan_data& data) //-V688 //-V818
    {
        this->ent = ent;
        this->record = record;
        this->data = data;
        this->distance = eyepos.DistTo(end_pos);
        this->health = health;
        this->damage = damage;
        this->fov = std::hypotf(viewangles.x - data.point.point.x, viewangles.y - data.point.point.y);
    }

    void reset()
    {
        ent = nullptr;
        record = nullptr;
        data.reset();
        fov = 0.0f;
        distance = 0.0f;
        health = 0;
        damage = 0;
    }
};


Vector_t vlast_shoot_position;
std::vector <legit_scanned_target> legit_scanned_targets;
legit_scanned_target final_target;
C_CSPlayerPawn* legit_last_target;

std::vector <legit_target> legit_targets;

// setup config hitboxes
void F::LEGIT::impl::Scan() {

    /* emplace menu hitboxes which will be used for hitscan*/

    if (legit_data.hitbox_head) {
        e_hitboxes.emplace_back(HEAD);
    }

    if (legit_data.hitbox_chest) {
        e_hitboxes.emplace_back(CHEST);
        e_hitboxes.emplace_back(RIGHT_CHEST);
        e_hitboxes.emplace_back(LEFT_CHEST);
    }

    if (legit_data.hitbox_stomach) {
        e_hitboxes.emplace_back(STOMACH);
        e_hitboxes.emplace_back(CENTER);
        e_hitboxes.emplace_back(PELVIS);
    }

    if (legit_data.hitbox_legs) {
        e_hitboxes.emplace_back(L_LEG);
        e_hitboxes.emplace_back(R_LEG);
    }
    if (legit_data.hitbox_feets) {
        e_hitboxes.emplace_back(L_FEET);
        e_hitboxes.emplace_back(R_FEET);
    }

    return;
}
void F::LEGIT::impl::Reset(reset type) {
    switch (type) {
    case reset::entity:
        legit_targets.clear();
        break;
    case reset::aimbot:
        e_hitboxes.clear();
        legit_targets.clear();
        legit_scanned_targets.clear();
        break;
    }
    return;
}

void F::LEGIT::impl::SetupTarget(C_CSPlayerPawn* pLocal)
{
    if (!D::pDrawListActive)
        return;

    if (!I::Engine->IsInGame()) return;

    CCSPlayerController* pLocalController = CCSPlayerController::GetLocalPlayerController();
    if (!pLocalController)
        return;

    pLocal = I::GameResourceService->pGameEntitySystem->Get<C_CSPlayerPawn>(pLocalController->GetPawnHandle());
    if (!pLocal)
        return;
    const std::lock_guard<std::mutex> guard{ g_cachedEntitiesMutex };

    for (const auto& it : g_cachedEntities) {

        C_BaseEntity* pEntity = I::GameResourceService->pGameEntitySystem->Get(it.m_handle);
        if (pEntity == nullptr)
            continue;

        CBaseHandle hEntity = pEntity->GetRefEHandle();
        if (hEntity != it.m_handle) continue;

        if (it.m_type != CachedEntity_t::PLAYER_CONTROLLER)
            continue;

        CCSPlayerController* CPlayer = I::GameResourceService->pGameEntitySystem->Get<CCSPlayerController>(hEntity);
        if (CPlayer == nullptr)
            break;

        C_CSPlayerPawn* player = I::GameResourceService->pGameEntitySystem->Get<C_CSPlayerPawn>(CPlayer->GetPawnHandle());
        if (!player)
            continue;

        if (player->GetHealth() <= 0 || !pLocal->IsOtherEnemy(player) || player->GetGameSceneNode()->IsDormant() || player->m_bGunGameImmunity())
            continue;

        legit_targets.push_back(legit_target(player, pEntity));
        continue;
    }



}

bool F::LEGIT::impl::valid(C_CSPlayerPawn* player, C_CSPlayerPawn* pLocal, bool check) {
    if (!player)
        return false;

    bool Invalid = player->GetHealth() <= 0 || player->GetLifeState() == ELifeState::LIFE_DISCARDBODY || player->GetLifeState() == ELifeState::LIFE_DEAD || player->GetLifeState() == ELifeState::LIFE_DYING;
    if (Invalid || !pLocal->IsOtherEnemy(player) || player->GetGameSceneNode()->IsDormant() || player->m_bGunGameImmunity())
        return false;

    return true;
}

static Vector_t get_target_angle(C_CSPlayerPawn* localplayer, Vector_t position)
{
    Vector_t eye_position = localplayer->GetEyePosition();
    Vector_t angle = position;

    angle.x = position.x - eye_position.x;
    angle.y = position.y - eye_position.y;
    angle.z = position.z - eye_position.z;

    angle.Normalizes();
    MATH::vec_angles(angle, &angle);

    angle.clamp();
    return angle;
}




void F::LEGIT::impl::ScanPoint(C_CSPlayerPawn* pLocal, CUserCmd* cmd, C_CSPlayerPawn* record, legit_scan_data& data, const Vector_t& shoot_pos, bool optimized) {
    auto weapon = pLocal->ActiveWeapon();
    if (!weapon)
        return;

    auto weapon_info = weapon->datawep();

    if (!weapon_info)
        return;

    if (e_hitboxes.empty())
        return;

    early_stopped = false;

    auto best_damage = 0;

    auto minimum_damage = 1.f;

    float damage = 0.f;
    bool can_hit = record->Visible(pLocal);

    std::vector <legit_scan_point> points;
   // std::vector <temp_point> temp_points;

    for (auto& hitbox : e_hitboxes)
    {
        
        float hitbox_scale = {}; QAngle_t temp_angle = {};  Vector_t hitbox_pos = {};  Vector4D_t hitbox_rot = {}; QAngle_t point_angle = {}; QAngle_t angle = {};

        // create our ange based on our current point
        record->CalculateHitboxData(hitbox, hitbox_pos, hitbox_rot, hitbox_scale, true);

        // shit got in vec convert to a quarention angle
        auto vec = get_target_angle(pLocal, hitbox_pos);
        angle.ToVec(vec);
        vec.clamp();

        // current point damage
        damage = legit->ScaleDamage(record, pLocal, weapon, hitbox_pos, damage, can_hit);

        MATH::VectorAngless(pLocal->GetEyePosition() - hitbox_pos, point_angle);

        // my idea is:
        // place tenmp points
        // keep subtick iterating ticks and placing angles to this point 
        // we that ^ we would never miss any tick or get any delay caused by subticks
        // the problem is that it isnt optimized nor properly done yet.
        /*if (damage <= 0) {
            temp_points.emplace_back(temp_point(hitbox_pos, point_angle, angle, hitbox, false, damage, can_hit));
            continue;
        }*/
        if (damage > 0) {
            points.emplace_back(legit_scan_point(hitbox_pos, vec, point_angle, angle, hitbox, false, damage, can_hit));
        }
    }

  /*  for (auto& temp_point : temp_points)
    {
        if (temp_point.damage > 0)
            continue;

        data.temp_point = temp_point;
    }
    */
    if (points.empty())
        return;

    auto current_minimum_damage = minimum_damage;

    for (auto& point : points)
    {
        if (point.damage <= 0)
            continue;

        if (point.damage >= current_minimum_damage && point.damage >= best_damage) {
            best_damage = point.damage;
            data.point = point;
            data.damage = point.damage;
            data.hitbox = point.hitbox;
            data.visible = point.canHit;
        }
    }
}


void F::LEGIT::impl::ScanTarget(C_CSPlayerPawn* pLocal, CUserCmd* cmd, QAngle_t viewangles)
{
    if (!pLocal)
        return;

    auto weapon = pLocal->ActiveWeapon();
    if (!weapon)
        return;

    if (!I::Engine->IsConnected() || !I::Engine->IsInGame())
        return;

    if (legit_targets.size() <= 0)
        return;

    final_target.reset();

    for (auto& target : legit_targets)
    {
        auto player = target.handle;

        if (!player)
            continue;

        if (!valid(player, pLocal))
            continue;

        legit_scan_data last_data;

        legit->ScanPoint(pLocal, cmd, player, last_data, Vector_t(0, 0, 0), true);

        legit_scanned_targets.emplace_back(legit_scanned_target(target.entHandle, viewangles, last_data.damage, player->GetHealth(), pLocal->GetEyePosition(), player->GetEyePosition(), player, last_data));
    
    }
}

bool F::LEGIT::impl::Ready(C_CSPlayerPawn* pLocal) {
    if (!pLocal)
        return false;

    auto ActiveWeapon = pLocal->ActiveWeapon();

    if (!ActiveWeapon)
        return false;

    auto data = ActiveWeapon->datawep();
    if (!data)
        return false;

    if (ActiveWeapon->clip1() <= 0)
        return false;


    return true;
}

float DistanceToCrosshair(const Vector_t& hitboxPos)
{
    auto center = ImGui::GetIO().DisplaySize * 0.5f;
    ImVec2 out;
    auto screenPos = D::WorldToScreen(hitboxPos, out);

    if (screenPos) {
        ImVec2 crosshairPos = center;
        ImVec2 hitboxScreenPos = out;

        float deltaX = crosshairPos.x - hitboxScreenPos.x;
        float deltaY = crosshairPos.y - hitboxScreenPos.y;

        return std::sqrt(deltaX * deltaX + deltaY * deltaY);
    }

    return FLT_MAX;
}
void F::LEGIT::impl::Events(IGameEvent* ev, events type) {
    if (!C_GET(bool, Vars.legit_enable))
        return;

    if (!I::Engine->IsConnected() || !I::Engine->IsInGame())
        return;

    if (!SDK::LocalController)
        return;

    switch (type) {
    case player_death: {
        auto controller = SDK::LocalController;
        if (!controller)
            break;

        const auto event_controller = ev->get_player_controller(CS_XOR("attacker"));
        if (!event_controller)
            return;


        if (event_controller->GetIdentity()->GetIndex() == controller->GetIdentity()->GetIndex()) {
            const std::int64_t value{ ev->get_int(CS_XOR("dmg_health")) };
            // reset targets
            legit_scanned_targets.clear();
            legit_targets.clear();
        }
    }
                     break;
    case round_start: {
        legit->Reset(reset::entity);
    }
                    break;
    }
}



inline Vector_t CrossProduct(const Vector_t& a, const Vector_t& b)
{
    return Vector_t(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

#define SMALL_NUM 0.00000001 // anything that avoids division overflow
#define dot(u, v) ((u).x * (v).x + (u).y * (v).y + (u).z * (v).z)
#define norm(v) sqrt(dot(v, v)) // norm = length of  vector
void VectorAngless(const Vector_t& forward, Vector_t& up, Vector_t& angles)
{
    Vector_t left = CrossProduct(up, forward);
    left.NormalizeInPlace();

    float forwardDist = forward.Length2D();

    if (forwardDist > 0.001f)
    {
        angles.x = atan2f(-forward.z, forwardDist) * 180 / MATH::_PI;
        angles.y = atan2f(forward.y, forward.x) * 180 / MATH::_PI;

        float upZ = (left.y * forward.x) - (left.x * forward.y);
        angles.z = atan2f(left.z, upZ) * 180 / MATH::_PI;
    }
    else
    {
        angles.x = atan2f(-forward.z, forwardDist) * 180 / MATH::_PI;
        angles.y = atan2f(-left.x, left.y) * 180 / MATH::_PI;
        angles.z = 0;
    }
}

void sin_coss(float radian, float* sin, float* cos)
{
    *sin = std::sin(radian);
    *cos = std::cos(radian);
}

float GRD_TO_BOGG(float GRD)
{
    return (MATH::_PI / 180) * GRD;
}
void AngleVectorss(const Vector_t& angles, Vector_t* forward)
{
    // sry
    //Assert(s_bMathlibInitialized);
    //Assert(forward);

    float sp, sy, cp, cy;

    sy = sin(DEG2RAD(angles[1]));
    cy = cos(DEG2RAD(angles[1]));

    sp = sin(DEG2RAD(angles[0]));
    cp = cos(DEG2RAD(angles[0]));

    forward->x = cp * cy;
    forward->y = cp * sy;
    forward->z = -sp;
}

void AngleVectorss(const Vector_t& angles, Vector_t* forward, Vector_t* right, Vector_t* up)
{
    float sp, sy, sr, cp, cy, cr;

    sin_coss(GRD_TO_BOGG(angles.x), &sp, &cp);
    sin_coss(GRD_TO_BOGG(angles.y), &sy, &cy);
    sin_coss(GRD_TO_BOGG(angles.z), &sr, &cr);

    if (forward != nullptr)
    {
        forward->x = cp * cy;
        forward->y = cp * sy;
        forward->z = -sp;
    }

    if (right != nullptr)
    {
        right->x = -1 * sr * sp * cy + -1 * cr * -sy;
        right->y = -1 * sr * sp * sy + -1 * cr * cy;
        right->z = -1 * sr * cp;
    }

    if (up != nullptr)
    {
        up->x = cr * sp * cy + -sr * -sy;
        up->y = cr * sp * sy + -sr * cy;
        up->z = cr * cp;
    }
}



float F::LEGIT::impl::GetSpread(C_CSWeaponBase* weapon)
{
    return weapon->get_spread();
}

void F::LEGIT::impl::BuildSeed()
{
    for (auto i = 0; i <= 255; i++) {
        MATH::fnRandomSeed(i + 1);

        const auto pi_seed = MATH::fnRandomFloat(0.f, 6.283186f);

        legit->m_computed_seeds.emplace_back(MATH::fnRandomFloat(0.f, 1.f),
            pi_seed);
    }
}

float F::LEGIT::impl::GetInaccuracy(C_CSPlayerPawn* pLocal, C_CSWeaponBase* weapon)
{
    return weapon->get_inaccuracy();
}

void NormalizeAngless(Vector_t& angles)
{
    for (auto i = 0; i < 3; i++)
    {
        while (angles[i] < -180.0f)
            angles[i] += 360.0f;
        while (angles[i] > 180.0f)
            angles[i] -= 360.0f;
    }
}


__forceinline Vector_t CalculateSpread(C_CSWeaponBase* weapon, int seed, float inaccuracy, float spread, bool revolver2 = false) {
    const char* item_def_index;
    float      recoil_index, r1, r2, r3, r4, s1, c1, s2, c2;

    if (!weapon)
        return { };
    // if we have no bullets, we have no spread.
    auto wep_info = weapon->datawep();
    if (!wep_info)
        return { };

    // get some data for later.
    item_def_index = wep_info->m_szName();
    recoil_index = weapon->m_flRecoilIndex();

    MATH::fnRandomSeed((seed & 0xff) + 1);

    // generate needed floats.
    r1 = MATH::fnRandomFloat(0.f, 1.f);
    r2 = MATH::fnRandomFloat(0.f, 3.14159265358979323846264338327950288f * 2);
    r3 = MATH::fnRandomFloat(0.f, 1.f);
    r4 = MATH::fnRandomFloat(0.f, 3.14159265358979323846264338327950288f * 2);

    // revolver secondary spread.
    if (item_def_index == CS_XOR("weapon_revoler") && revolver2) {
        r1 = 1.f - (r1 * r1);
        r3 = 1.f - (r3 * r3);
    }

    // negev spread.
    else if (item_def_index == CS_XOR("weapon_negev") && recoil_index < 3.f) {
        for (int i{ 3 }; i > recoil_index; --i) {
            r1 *= r1;
            r3 *= r3;
        }

        r1 = 1.f - r1;
        r3 = 1.f - r3;
    }

    // get needed sine / cosine values.
    c1 = std::cos(r2);
    c2 = std::cos(r4);
    s1 = std::sin(r2);
    s2 = std::sin(r4);

    // calculate spread vector.
    return {
        (c1 * (r1 * inaccuracy)) + (c2 * (r3 * spread)),
        (s1 * (r1 * inaccuracy)) + (s2 * (r3 * spread)),
        0.f
    };
}


void VectorAngless2(const Vector_t& forward, Vector_t& angles)
{
    float tmp, yaw, pitch;

    if (forward[1] == 0 && forward[0] == 0)
    {
        yaw = 0;
        if (forward[2] > 0)
            pitch = 270;
        else
            pitch = 90;
    }
    else
    {
        yaw = (atan2(forward[1], forward[0]) * 180 / M_PI);
        if (yaw < 0)
            yaw += 360;

        tmp = sqrt(forward[0] * forward[0] + forward[1] * forward[1]);
        pitch = (atan2(-forward[2], tmp) * 180 / M_PI);
        if (pitch < 0)
            pitch += 360;
    }

    angles[0] = pitch;
    angles[1] = yaw;
    angles[2] = 0;
}
// Linear interpolation function
template <typename T>
T Lerp(float t, const T& start, const T& end) {
    return start + t * (end - start);
}

const char* GetExtractedWeaponNames(C_CSWeaponBase* weapon)
{
    if (!weapon)
        return "";

    auto weapon_data = weapon->datawep();
    if (!weapon_data)
        return "";

    const char* szWeaponName = weapon_data->m_szName();
    const char* weaponPrefix = ("weapon_");
    const char* weaponNameStart = strstr(szWeaponName, weaponPrefix);
    const char* extractedWeaponName = weaponNameStart ? weaponNameStart + strlen(weaponPrefix) : szWeaponName;

    return extractedWeaponName;
}


static bool compare_targets(const legit_scanned_target& first, const legit_scanned_target& second)
{
    // higher damage is preferred
    if (first.damage > second.damage)
        return true;
    else if (first.damage < second.damage)
        return false;

    // lower distance is preferred if damages are equal
    return first.distance < second.distance;
}

void F::LEGIT::impl::SortTarget(QAngle_t viewangles)
{
    const float best_fov = (float)legit_data.fov;
    auto validate_target = [&](legit_scanned_target* a, legit_scanned_target* b) -> bool {
        if (!a || !b)
            goto fuck_yeah;


        if (legit_last_target != nullptr && legit_last_target->GetHealth() <= 0 && a->record->GetIdentity()->GetIndex() != legit_last_target->GetIdentity()->GetIndex() && legit_last_target->GetIdentity()->GetIndex() <= 64) {
            return true;
        }


        // as we are legit botting prefer only by crosshair always 
        return DistanceToCrosshair(a->data.point.pointangle) > DistanceToCrosshair(b->data.point.pointangle);

    fuck_yeah:
        // this might not make sense to you, but it actually does.
        return (a != nullptr || (a != nullptr && b != nullptr && a == b)) ? true : false;
    };

    for (auto& data : legit_scanned_targets) {
        auto fov = std::hypotf(viewangles.x - data.data.point.angle.x, viewangles.y - data.data.point.angle.y);
      
        if (data.damage < 1.f || fov > best_fov)
            continue;

        if (!final_target.record) {
            final_target = data;

            // we only have one entry (target)? let's skip target selection..
            if (legit_scanned_targets.size() == 1)
                break;
            else
                continue;
        }

        if (legit_last_target != nullptr && legit_last_target->GetHealth() <= 0 && data.record->GetIdentity()->GetIndex() != legit_last_target->GetIdentity()->GetIndex()) {
            continue;
        }

        // sort our target based on our conditions
        if (validate_target(&data, &final_target)) {
            final_target = data;
            continue;
        }
    }

}

float CalculateFactor(float distance, float maxDistance, float minFactor, float maxFactor)
{
    // Ensure distance is within valid range
    if (distance > maxDistance)
        distance = maxDistance;

    // Calculate the factor based on the distance
    float factor = minFactor + (maxFactor - minFactor) * (1 - distance / maxDistance);

    return factor;
}
QAngle_t F::LEGIT::impl::PerformSmooth(QAngle_t currentAngles, const QAngle_t& targetAngles, float smoothFactor, C_CSPlayerPawn* pLocal, Vector_t hitboxPos)
{
    QAngle_t smoothedAngles;
    float effectiveSmoothFactor  = smoothFactor * 10.0f;
    // Calculate the difference between target and current angles
    QAngle_t angleDiff = targetAngles - currentAngles;

    // Apply acceleration and deceleration
    smoothedAngles.x = currentAngles.x + (angleDiff.x / effectiveSmoothFactor);
    smoothedAngles.y = currentAngles.y + (angleDiff.y / effectiveSmoothFactor);

    // Ensure angles are within valid range
    smoothedAngles.Clamp();

    return smoothedAngles;
}


void  F::LEGIT::impl::Triggerbot(CUserCmd* cmd, C_BaseEntity* localent, C_BaseEntity* playerent, C_CSPlayerPawn* local, C_CSPlayerPawn* player, CCSWeaponBaseVData* vdata)
{
    VIRTUALIZER_MUTATE_ONLY_START;
    if (!legit_data.triggerbot)
        return;

    if (!SDK::LocalController)
        return;

    auto weapon = local->ActiveWeapon();
    if (!weapon)
        return;

    auto data = weapon->datawep();
    if (!data)
        return;

    if (!cmd)
        return;

    auto base = cmd->m_csgoUserCmd.m_pBaseCmd;
    if (!base)
        return;


    bool should_trigger = player->InsideCrosshair(local, base->m_pViewangles->m_angValue, data->m_flRange());

    if (should_trigger) {
            cmd->m_nButtons.m_nValue |= IN_ATTACK;
    }
    //F::AUTOWALL::c_auto_wall::data_t pendata;
    //F::AUTOWALL::g_auto_wall->pen(pendata, vecStart, vecEnd, playerent, localent, local, player, vdata, pen_dmg, valid);
    // check do we can shoot
    VIRTUALIZER_MUTATE_ONLY_END;

}
float F::LEGIT::impl::ScaleDamage(C_CSPlayerPawn* target, C_CSPlayerPawn* pLocal, C_CSWeaponBase* weapon, Vector_t aim_point, float& dmg, bool& canHit)
{
    if (!pLocal || !weapon || !target)
        return 0.f;

    if (pLocal->GetHealth() <= 0)
        return 0.f;

    auto vdata = weapon->datawep();
    if (!vdata)
        return 0.f;

    auto entity = I::GameResourceService->pGameEntitySystem->Get(target->GetRefEHandle());
    if (!entity)
        return 0.f;

    auto localent = I::GameResourceService->pGameEntitySystem->Get(pLocal->GetRefEHandle());
    if (!localent)
        return 0.f;

    float damage = 0.f;
    F::AUTOWALL::c_auto_wall::data_t data;
    F::AUTOWALL::g_auto_wall->pen(data, pLocal->GetEyePosition(), aim_point, entity, localent, pLocal, target, vdata, damage, canHit);
    return data.m_can_hit ? data.m_dmg : 0.f;
}

void F::LEGIT::impl::Run(C_CSPlayerPawn* pLocal, CCSGOInput* pInput, CUserCmd* cmd) {

    if (!C_GET(bool, Vars.legit_enable))
        return;

    if (!I::Engine->IsConnected() || !I::Engine->IsInGame())
        return;

    if (!SDK::LocalController || !SDK::LocalPawn)
        return;

    // sanity check
    if (!pLocal || pLocal->GetHealth() <= 0) {
        legit_last_target = nullptr;
        return;
    }

    if (!pInput || !cmd)
        return;

    auto pCmd = cmd->m_csgoUserCmd.m_pBaseCmd;
    if (!pCmd)
        return;

    legit->Reset(reset::aimbot);
    auto viewangles = pCmd->m_pViewangles->m_angValue;

    // setup menu adaptive weapon with legit data
    legit->SetupAdaptiveWeapon(pLocal);

    // no ammo or not valid weapon 
    if (!legit->Ready(pLocal))
        return;

    VIRTUALIZER_TIGER_WHITE_START

        // hitbox menu selection 
        legit->Scan();

    // store targets in server
    legit->SetupTarget(pLocal);

    // keep scanned targets getting updated 
    legit_scanned_targets.clear();

    // scan & select them based on our conditions
    legit->ScanTarget(pLocal, cmd, pCmd->m_pViewangles->m_angValue);

    if (legit_scanned_targets.empty())
        return;

    auto weapon_data = pLocal->ActiveWeapon();
    if (!weapon_data)
        return;

    auto vdata = weapon_data->datawep();
    if (!vdata)
        return;

    if (legit_last_target != nullptr && legit_last_target->GetHealth() <= 0) {
        legit_last_target = nullptr;
    }

    // select best target
    legit->SortTarget(    pCmd->m_pViewangles->m_angValue);

    if (!(final_target.data.valid()))
        return;

    auto best_target = final_target;

    // calculate aimpunch & compensate it 
    static auto prev = QAngle_t(0.f, 0.f, 0.f);
    auto cache = pLocal->m_aimPunchCache();
    auto pred_punch = cache.m_Data[cache.m_Size - 1];
    auto delta = prev - pred_punch * 2.f;
    if (cache.m_Size > 0 && cache.m_Size <= 0xFFFF) {
        pred_punch = cache.m_Data[cache.m_Size - 1];
        prev = pred_punch;
    }


    auto best_point = final_target.data.point.angle + delta * 2.f;
    auto fov = std::hypotf(    pCmd->m_pViewangles->m_angValue.x - final_target.data.point.angle.x,     pCmd->m_pViewangles->m_angValue.y - final_target.data.point.angle.y);
    auto silent_limit = 25.f;

    sub_tickdata.command = commands_msg::none;

    if (legit_data.triggerbot)
        Triggerbot(cmd, nullptr, nullptr, pLocal, final_target.record, vdata);

    if (GetAsyncKeyState(C_GET(KeyBind_t, Vars.legit_key).uKey))
    {
        if (legit_data.perfect_silent && fov < silent_limit) {
            // setup the message we will send to IntupHistory
            sub_tickdata.best_point = best_point + delta * 2.f;
            sub_tickdata.best_point_vec = final_target.data.point.point;

            // apply viewangles command
             sub_tickdata.command = commands_msg::viewangles;




        }
        else {
            int smooth = legit_data.smooth;
            QAngle_t smoothedAnglesRCS = PerformSmooth(viewangles, best_point, static_cast<float>(smooth), pLocal, final_target.data.point.point);

                pCmd->m_pViewangles->m_angValue = smoothedAnglesRCS;
                pCmd->m_pViewangles->m_angValue.Clamp();
        }
    }


    legit_last_target = final_target.record;

    VIRTUALIZER_MUTATE_ONLY_END;
}

void F::LEGIT::impl::SetupAdaptiveWeapon(C_CSPlayerPawn* pLocal) {
    if (!C_GET(bool, Vars.legit_enable))
        return;

    if (!pLocal)
        return;

    if (pLocal->GetHealth() <= 0)
        return;


    auto ActiveWeapon = pLocal->ActiveWeapon();
    if (!ActiveWeapon)
        return;

    auto data = ActiveWeapon->datawep();
    if (!data)
        return;

    if (!ActiveWeapon->IsWeapon())
        return;

    const char* extractedWeaponName = GetExtractedWeaponNames(ActiveWeapon);

    bool has_awp = strcmp(extractedWeaponName, CS_XOR("awp")) == 0;
    bool has_heavy_pistols = strcmp(extractedWeaponName, CS_XOR("revolver")) == 0 || strcmp(extractedWeaponName, CS_XOR("deagle")) == 0;
    bool has_scout = strcmp(extractedWeaponName, CS_XOR("ssg08")) == 0;
    legit_data.legit_enable = C_GET(bool, Vars.legit_enable);
    legit_data.key_enabled = C_GET(KeyBind_t, Vars.legit_key).bEnable;

    if (has_awp) {
        legit_data.rcs_control = C_GET_ARRAY(int, 7, Vars.legit_rcs, 6);
        legit_data.rcs_x = C_GET_ARRAY(int, 7, Vars.legit_rcs_smoothx_pistol, 6);
        legit_data.rcs_y = C_GET_ARRAY(bool, 7, Vars.legit_rcs_smoothx_pistol, 6);
        legit_data.perfect_silent = C_GET_ARRAY(bool, 7, Vars.perfect_silent, 6);
        legit_data.triggerbot = C_GET_ARRAY(bool, 7, Vars.legit_triggerbot, 6);
        legit_data.smooth = C_GET_ARRAY(int, 7, Vars.legit_smooth, 6);
        legit_data.smoothing = C_GET_ARRAY(bool, 7, Vars.legit_smoothing, 6);
        legit_data.hitbox_head = C_GET_ARRAY(bool, 7, Vars.hitbox_head, 6);
        legit_data.hitbox_stomach = C_GET_ARRAY(bool, 7, Vars.hitbox_chest, 6);
        legit_data.hitbox_chest = C_GET_ARRAY(bool, 7, Vars.hitbox_stomach, 6);
        legit_data.hitbox_legs = C_GET_ARRAY(bool, 7, Vars.hitbox_legs, 6);
        legit_data.fov = C_GET_ARRAY(float, 7, Vars.legit_fov, 6);
    }
    else if (has_scout) {
        legit_data.rcs_control = C_GET_ARRAY(int, 7, Vars.legit_rcs, 5);
        legit_data.rcs_x = C_GET_ARRAY(int, 7, Vars.legit_rcs_smoothx_pistol, 5);
        legit_data.rcs_y = C_GET_ARRAY(bool, 7, Vars.legit_rcs_smoothx_pistol, 5);
        legit_data.triggerbot = C_GET_ARRAY(bool, 7, Vars.legit_triggerbot, 5);
        legit_data.smooth = C_GET_ARRAY(int, 7, Vars.legit_smooth, 5);
        legit_data.smoothing = C_GET_ARRAY(bool, 7, Vars.legit_smoothing, 5);
        legit_data.hitbox_head = C_GET_ARRAY(bool, 7, Vars.hitbox_head, 5);
        legit_data.hitbox_stomach = C_GET_ARRAY(bool, 7, Vars.hitbox_chest, 5);
        legit_data.hitbox_chest = C_GET_ARRAY(bool, 7, Vars.hitbox_stomach, 5);
        legit_data.perfect_silent = C_GET_ARRAY(bool, 7, Vars.perfect_silent, 5);

        legit_data.hitbox_legs = C_GET_ARRAY(bool, 7, Vars.hitbox_legs, 5);
        legit_data.fov = C_GET_ARRAY(float, 7, Vars.legit_fov, 5);

    }
    else if (has_heavy_pistols) {
        legit_data.rcs_control = C_GET_ARRAY(int, 7, Vars.legit_rcs, 2);
        legit_data.rcs_x = C_GET_ARRAY(int, 7, Vars.legit_rcs_smoothx_pistol, 2);
        legit_data.perfect_silent = C_GET_ARRAY(bool, 7, Vars.perfect_silent, 2);

        legit_data.rcs_y = C_GET_ARRAY(bool, 7, Vars.legit_rcs_smoothx_pistol, 2);
        legit_data.triggerbot = C_GET_ARRAY(bool, 7, Vars.legit_triggerbot, 2);
        legit_data.smooth = C_GET_ARRAY(int, 7, Vars.legit_smooth, 2);
        legit_data.smoothing = C_GET_ARRAY(bool, 7, Vars.legit_smoothing, 2);
        legit_data.hitbox_head = C_GET_ARRAY(bool, 7, Vars.hitbox_head, 2);
        legit_data.hitbox_stomach = C_GET_ARRAY(bool, 7, Vars.hitbox_chest, 2);
        legit_data.hitbox_chest = C_GET_ARRAY(bool, 7, Vars.hitbox_stomach, 2);
        legit_data.hitbox_legs = C_GET_ARRAY(bool, 7, Vars.hitbox_legs, 2);
        legit_data.fov = C_GET_ARRAY(float, 7, Vars.legit_fov, 2);

    }
    else if (data->m_WeaponType() == WEAPONTYPE_PISTOL && !has_heavy_pistols) {
        legit_data.rcs_control = C_GET_ARRAY(int, 7, Vars.legit_rcs, 1);
        legit_data.perfect_silent = C_GET_ARRAY(bool, 7, Vars.perfect_silent, 1);

        legit_data.rcs_x = C_GET_ARRAY(int, 7, Vars.legit_rcs_smoothx_pistol, 1);
        legit_data.rcs_y = C_GET_ARRAY(bool, 7, Vars.legit_rcs_smoothx_pistol, 1);
        legit_data.triggerbot = C_GET_ARRAY(bool, 7, Vars.legit_triggerbot, 1);
        legit_data.smooth = C_GET_ARRAY(int, 7, Vars.legit_smooth, 1);
        legit_data.smoothing = C_GET_ARRAY(bool, 7, Vars.legit_smoothing, 1);
        legit_data.hitbox_head = C_GET_ARRAY(bool, 7, Vars.hitbox_head, 1);
        legit_data.hitbox_stomach = C_GET_ARRAY(bool, 7, Vars.hitbox_chest, 1);
        legit_data.hitbox_chest = C_GET_ARRAY(bool, 7, Vars.hitbox_stomach, 1);
        legit_data.hitbox_legs = C_GET_ARRAY(bool, 7, Vars.hitbox_legs, 1);
        legit_data.fov = C_GET_ARRAY(float, 7, Vars.legit_fov, 1);

    }
    else if (data->m_WeaponType() == WEAPONTYPE_MACHINEGUN || data->m_WeaponType() == WEAPONTYPE_RIFLE) {
        legit_data.rcs_control = C_GET_ARRAY(int, 7, Vars.legit_rcs, 3);
        legit_data.rcs_x = C_GET_ARRAY(int, 7, Vars.legit_rcs_smoothx_pistol, 3);
        legit_data.perfect_silent = C_GET_ARRAY(bool, 7, Vars.perfect_silent, 3);

        legit_data.rcs_y = C_GET_ARRAY(bool, 7, Vars.legit_rcs_smoothx_pistol, 3);
        legit_data.triggerbot = C_GET_ARRAY(bool, 7, Vars.legit_triggerbot, 3);
        legit_data.smooth = C_GET_ARRAY(int, 7, Vars.legit_smooth, 3);
        legit_data.smoothing = C_GET_ARRAY(bool, 7, Vars.legit_smoothing, 3);
        legit_data.hitbox_head = C_GET_ARRAY(bool, 7, Vars.hitbox_head, 3);
        legit_data.hitbox_stomach = C_GET_ARRAY(bool, 7, Vars.hitbox_chest, 3);
        legit_data.hitbox_chest = C_GET_ARRAY(bool, 7, Vars.hitbox_stomach, 3);
        legit_data.hitbox_legs = C_GET_ARRAY(bool, 7, Vars.hitbox_legs, 3);
        legit_data.fov = C_GET_ARRAY(float, 7, Vars.legit_fov, 3);

    }
    else if (data->m_WeaponType() == WEAPONTYPE_SNIPER_RIFLE) {
        legit_data.rcs_control = C_GET_ARRAY(int, 7, Vars.legit_rcs, 4);
        legit_data.rcs_x = C_GET_ARRAY(int, 7, Vars.legit_rcs_smoothx_pistol, 4);
        legit_data.rcs_y = C_GET_ARRAY(bool, 7, Vars.legit_rcs_smoothx_pistol, 4);
        legit_data.perfect_silent = C_GET_ARRAY(bool, 7, Vars.perfect_silent, 4);

        legit_data.triggerbot = C_GET_ARRAY(bool, 7, Vars.legit_triggerbot, 4);
        legit_data.smooth = C_GET_ARRAY(int, 7, Vars.legit_smooth, 4);
        legit_data.smoothing = C_GET_ARRAY(bool, 7, Vars.legit_smoothing, 4);
        legit_data.hitbox_head = C_GET_ARRAY(bool, 7, Vars.hitbox_head, 4);
        legit_data.hitbox_stomach = C_GET_ARRAY(bool, 7, Vars.hitbox_chest, 4);
        legit_data.hitbox_chest = C_GET_ARRAY(bool, 7, Vars.hitbox_stomach, 4);
        legit_data.hitbox_legs = C_GET_ARRAY(bool, 7, Vars.hitbox_legs, 4);
        legit_data.fov = C_GET_ARRAY(float, 7, Vars.legit_fov, 4);


    }

}

