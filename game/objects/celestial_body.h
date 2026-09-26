#include "engine/core/game_object.h"
#include "engine/core/interface/updatable.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/physics/contracts/physics_step_participant.h"

namespace game::objcets
{

    class CelestialBody : public elysia::core::GameObject,
        public elysia::core::Updatable,
        public elysia::physics::PhysicsParticipant,
        public elysia::physics::PhysicsStepParticipant

    {

    public:
    };

}