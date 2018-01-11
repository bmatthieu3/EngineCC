#include <iostream>
#include <vector>
#include <memory>

#include "Game.h"
#include "World.h"
#include "GameProgram.h"
#include "InputHandler.h"
#include "Manager.h"

#include "Components.h"

#include "RenderSystem.h"
#include "PhysicSystem.h"
#include "ScriptSystem.h"
#include "PickingSystem.h"

#include <entityx/entityx.h>
#include <glm/gtx/vector_angle.hpp>

/// Event Definitions
struct DisplacementEvent {
	DisplacementEvent(entityx::Entity entity, const glm::vec3& direction) : entity(entity),
																			direction(direction) {
	}
	// The entity to move
	entityx::Entity entity;
	glm::vec3 direction;
};

struct StopDisplacementEvent {
	StopDisplacementEvent(entityx::Entity entity) : entity(entity) {
	}

	entityx::Entity entity;
};

/// System Definitions
class MovementSystem : public entityx::System<MovementSystem>, public entityx::Receiver<MovementSystem> {
public:
	MovementSystem() {
	}
	~MovementSystem() {
	}

	void update(entityx::EntityManager &es, entityx::EventManager &events, entityx::TimeDelta dt) override {
		es.each<Physics, Movable>([this, &dt](entityx::Entity entity, Physics& physic, Movable& movable) {
			// Movable entities are stopped until we receive a new MovementEvent. 
			const btVector3& velocity = physic.rigid_body->getLinearVelocity();
			physic.rigid_body->setLinearVelocity(btVector3(0, velocity.y(), 0));
			if (m_movements.find(entity) != m_movements.end()) {
				// If a MovementEvent has been retrieved we set the new linear velocity towards the direction 
				// defined in the Event and at the speed defined in the MovableComponent
				btVector3 new_velocity(m_movements[entity].velocity * movable.speed);
				
				physic.rigid_body->setLinearVelocity(new_velocity);
			}
		});
	}

	void configure(entityx::EventManager &events) override {
		events.subscribe<DisplacementEvent>(*this);
		events.subscribe<StopDisplacementEvent>(*this);
	}
	void receive(const DisplacementEvent& move) {
		Movement movement = { btVector3(move.direction.x, 0.f, move.direction.z), 1.0f };
		m_movements[move.entity] = movement;
	}
	void receive(const StopDisplacementEvent& stop_move) {
		m_movements.erase(stop_move.entity);
	}
private:
	struct Movement {
		btVector3 velocity;
		float time;
	};

	std::map<entityx::Entity, Movement> m_movements;
};

void Game::createPlayerEntity(entityx::EntityManager &es, World& world) {
	entityx::Entity entity = es.create();

	// Render Component
	Manager<std::string, std::shared_ptr<Shader>>& shaders = Manager<std::string, std::shared_ptr<Shader>>::getInstance();
	std::shared_ptr<Renderable<Cube>> ground_render = std::make_shared<Renderable<Cube>>(shaders.get("simple"));

	LocalTransform tr;
	ground_render->setLocalTransform(tr);
	ground_render->setInvisible();
	entity.assign<Render>(ground_render);

	// Physics Component
	glm::vec3 size_box(1.5f, 2.5f, 1.5f);
	//btCollisionShape* entity_shape = new btBoxShape(btVector3(size_box.x / 2.f, size_box.y / 2.f, size_box.z / 2.f));
	btCollisionShape* entity_shape = new btCapsuleShape(size_box.x/2, size_box.y);
	btTransform entity_tr;
	entity_tr.setIdentity();
	entity_tr.setOrigin(btVector3(m_viewer.getPosition().x, m_viewer.getPosition().y, m_viewer.getPosition().z));
	btScalar mass(1.f);
	//rigidbody is dynamic if and only if mass is non zero, otherwise static

	btCompoundShape* compound = new btCompoundShape();
	compound->addChildShape(btTransform::getIdentity(), entity_shape);

	bool is_dynamic = (mass != 0.f);
	btVector3 local_inertia(0, 0, 0);
	if (is_dynamic)
		compound->calculateLocalInertia(mass, local_inertia);

	//using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
	btDefaultMotionState* motion_state = new btDefaultMotionState(entity_tr);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motion_state, compound, local_inertia);
	btRigidBody* body = new btRigidBody(rbInfo);
	// Active rigid body
	body->setActivationState(DISABLE_DEACTIVATION);

	// Disable angular rotation when hitting another body
	body->setAngularFactor(0);

	Physics physics = { compound, motion_state, body, mass, local_inertia, nullptr };
	entity.assign<Physics>(physics);

	// Movable Component
	float player_speed = 8.f;
	Movable movable(player_speed);
	entity.assign<Movable>(movable);

	// The player can carry two things at the same time, one entity for its left hand and a second for its right hand
	Handler handler;
	entity.assign<Handler>(handler);

	addEntity("player", entity);
}

// Creation of all the in-game entities in Game constructor's class
void Game::createGroundEntity(entityx::EntityManager &es) {
	entityx::Entity entity = es.create();

	// Render Component
	Manager<std::string, std::shared_ptr<Shader>>& shaders = Manager<std::string, std::shared_ptr<Shader>>::getInstance();
	std::shared_ptr<Renderable<Plane>> ground_render = std::make_shared<Renderable<Plane>>(shaders.get("texture"));
	ground_render->setTexture("C:\\Users\\Matthieu\\Source\\Repos\\EngineCC\\EngineCC\\EngineCC\\Content\\wall3.jpg");
#define STD_UNIT_PER_TILE 4

	LocalTransform tr;
	tr.setTranslation(glm::vec3(0, 0, 0));
	//tr.setScale(glm::vec3(1, 0, 1));
	ground_render->setLocalTransform(tr);
	entity.assign<Render>(ground_render);

	// Physics Component
	btCollisionShape* ground_shape = new btStaticPlaneShape(btVector3(0, 1, 0), 0);
	ground_shape->setLocalScaling(btVector3(100, 0, 100));
	ground_render->setTexcoordsFactor(glm::vec3(100 / STD_UNIT_PER_TILE, 100 / STD_UNIT_PER_TILE, 0));
	btTransform ground_tr;
	ground_tr.setIdentity();
	ground_tr.setOrigin(btVector3(0, 0, 0));
	btScalar mass(0.);
	//rigidbody is dynamic if and only if mass is non zero, otherwise static
	bool is_dynamic = (mass != 0.f);
	btVector3 local_inertia(0, 0, 0);
	if (is_dynamic)
		ground_shape->calculateLocalInertia(mass, local_inertia);

	//using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
	btDefaultMotionState* motion_state = new btDefaultMotionState(ground_tr);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motion_state, ground_shape, local_inertia);
	btRigidBody* body = new btRigidBody(rbInfo);
	
	body->setFriction(2.f);
	Physics physics = { ground_shape, motion_state, body, mass, local_inertia, nullptr };
	entity.assign<Physics>(physics);

	// Add an open script to the door

	addEntity("ground", entity);
}

// Creation of all the in-game entities in Game constructor's class
void Game::createDoorEntity(entityx::EntityManager &es, World& world) {
	entityx::Entity entity = es.create();

	// Render Component
	Manager<std::string, std::shared_ptr<Shader>>& shaders = Manager<std::string, std::shared_ptr<Shader>>::getInstance();
	std::shared_ptr<Renderable<Cube>> render = std::make_shared<Renderable<Cube>>(shaders.get("simple"));
	LocalTransform tr;

	float length = 4.f;
	float height = 6.f;
	float angle = M_PI / 4.f;

	//tr.setTranslation(glm::vec3(5, 0, 0));
	tr.setScale(glm::vec3(length, height, 0.4));
	//tr.setRotation(glm::vec3(0, 0, 1), 90*2*M_PI/360.f);
	render->setLocalTransform(tr);
	entity.assign<Render>(render);

	// Physics Component
	std::vector<glm::vec3>& vertices = render->getPrimitive().getVertices();

	/*btConvexHullShape* entity_shape = new btConvexHullShape();
	for (int i = 0; i < vertices.size(); ++i) {
		entity_shape->addPoint(btVector3(vertices[i].x, vertices[i].y, vertices[i].z));
	}
	entity_shape->optimizeConvexHull();*/
	btCollisionShape* entity_shape = new btBoxShape(btVector3(0.5, 0.5, 0.5));

	btTransform entity_tr;
	entity_tr.setIdentity();
	entity_tr.setOrigin(btVector3(cos(angle) * length / 2, height / 2, - sin(angle) * length / 2));
	entity_tr.setRotation(btQuaternion(angle, 0, 0));
	entity_shape->setLocalScaling(btVector3(length, height, 0.4));
	btScalar mass(1.);
	//rigidbody is dynamic if and only if mass is non zero, otherwise static
	bool is_dynamic = (mass != 0.f);
	btVector3 local_inertia(0, 0, 0);
	if (is_dynamic)
		entity_shape->calculateLocalInertia(mass, local_inertia);

	//using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
	btDefaultMotionState* motion_state = new btDefaultMotionState(entity_tr);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motion_state, entity_shape, local_inertia);
	btRigidBody* body = new btRigidBody(rbInfo);
	body->setActivationState(DISABLE_DEACTIVATION);

	// Add hinge constraint
	btVector3 pivot(-length / 2.f, 0, 0);
	btVector3 axis(0, 1, 0);

	btHingeConstraint* hinge_constraint = new btHingeConstraint(*body, pivot, axis);
	hinge_constraint->setLimit(angle - M_PI * 0.5, angle + M_PI * 0.5);

	/// For debug purposes
	hinge_constraint->setDbgDrawSize(btScalar(5.f));

	Physics physics = { entity_shape, motion_state, body, mass, local_inertia, hinge_constraint};
	entity.assign<Physics>(physics);

	// Script Component
	Manager<std::string, std::shared_ptr<FiniteStateMachine>>& scripts = Manager<std::string, std::shared_ptr<FiniteStateMachine>>::getInstance();
	Script script;
	script.m_scripts[Script::INTERACTION] = scripts.get("door_opening");
	entity.assign<Script>(script);

	addEntity("door", entity);
}

void Game::createSwordEntity(const std::string& name, entityx::EntityManager &es, World& world) {
	entityx::Entity entity = es.create();

	Manager<std::string, std::shared_ptr<Shader>>& shaders = Manager<std::string, std::shared_ptr<Shader>>::getInstance();
	std::shared_ptr<Renderable<Model>> arrow_render = std::make_shared<Renderable<Model>>(shaders.get("simple"), "C:\\Users\\Matthieu\\Source\\Repos\\EngineCC\\EngineCC\\EngineCC\\Content\\sword.obj");
	entity.assign<Render>(arrow_render);

	/// Add physic component
	// Collision shape computed from the mesh of the entity
	entityx::ComponentHandle<Render> render = entity.component<Render>();
	assert(*render != nullptr);
	std::vector<glm::vec3>& vertices = (*render)->getPrimitive().getVertices();

	btConvexHullShape* entity_shape = new btConvexHullShape();
	for (int i = 0; i < vertices.size(); ++i) {
		entity_shape->addPoint(btVector3(vertices[i].x, vertices[i].y, vertices[i].z));
	}
	entity_shape->optimizeConvexHull();

	btTransform entity_transform;
	entity_transform.setIdentity();
	entity_transform.setOrigin(btVector3(2, 3, 2));
	//rigidbody is dynamic if and only if mass is non zero, otherwise static
	float mass = 1.f;
	bool is_dynamic = (mass != 0.f);
	btVector3 local_inertia(0, 0, 0);
	if (is_dynamic)
		entity_shape->calculateLocalInertia(mass, local_inertia);

	//using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
	btDefaultMotionState* motion_state = new btDefaultMotionState(entity_transform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motion_state, entity_shape, local_inertia);
	btRigidBody* body = new btRigidBody(rbInfo);
	// Active rigid body
	body->setActivationState(DISABLE_DEACTIVATION);
	//if (data.disable_angular_rotation)
	//	body->setAngularFactor(0.f);

	Physics physics = { entity_shape, motion_state, body, mass, local_inertia, nullptr };
	entity.assign<Physics>(physics);

	// A sword is carryable
	btTransform local_transform;
	local_transform.setIdentity();
	local_transform.setOrigin(btVector3(0.3f, -0.5f, -0.5f));
	local_transform.setRotation(btQuaternion(0, M_PI / 2.f, 0));
	Carryable carryable = { "typical iron sword", "all humans, orks and dwarfs can recognize it ! It's THE sword", true, local_transform };
	entity.assign<Carryable>(carryable);

	addEntity(name, entity);
}

// Creation of all the in-game entities in Game constructor's class
void Game::createArrowEntity(entityx::EntityManager &es, entityx::EventManager &events) {
	entityx::Entity entity = es.create();

	Manager<std::string, std::shared_ptr<Shader>>& shaders = Manager<std::string, std::shared_ptr<Shader>>::getInstance();
	std::shared_ptr<Renderable<Cube>> arrow_render = std::make_shared<Renderable<Cube>>(shaders.get("simple"));
	LocalTransform tr;
	tr.setTranslation(glm::vec3(10, 5, 0));
	tr.setScale(glm::vec3(0.5, 0.5, 2));
	arrow_render->setLocalTransform(tr);
	entity.assign<Render>(arrow_render);

	// Physics Component
	const glm::vec3& scale = tr.getScaleVec();
	btCollisionShape* arrow_shape = new btBoxShape(btVector3(scale.x / 2.f, scale.y / 2.f, scale.z / 2.f));
	btTransform arrow_tr;
	arrow_tr.setIdentity();
	arrow_tr.setOrigin(btVector3(m_viewer.getPosition().x, m_viewer.getPosition().y, m_viewer.getPosition().z));
	btScalar mass(1.);
	//rigidbody is dynamic if and only if mass is non zero, otherwise static
	bool is_dynamic = (mass != 0.f);
	btVector3 local_inertia(0, 0, 0);
	if (is_dynamic)
		arrow_shape->calculateLocalInertia(mass, local_inertia);

	//using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
	btDefaultMotionState* motion_state = new btDefaultMotionState(arrow_tr);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motion_state, arrow_shape, local_inertia);
	btRigidBody* body = new btRigidBody(rbInfo);
	body->setLinearVelocity(btVector3(m_viewer.getDirection().x, m_viewer.getDirection().y, m_viewer.getDirection().z));

	Physics physics = { arrow_shape, motion_state, body, mass, local_inertia, nullptr };
	entity.assign<Physics>(physics);
	
	addEntity("arrow", entity);
}

void Game::init(entityx::EntityManager& es_editor) {
	entities.reset();

	World& world = Singleton<World>::getInstance();

	m_viewer.setPosition(glm::vec3(0, 5, 0));
	m_viewer.setDirection(glm::vec3(1, 0, 0));
	
	createGroundEntity(entities);
	createDoorEntity(entities, world);
	createSwordEntity("sword", entities, world);
	createSwordEntity("sword2", entities, world);

	// The player is instanciated in last after all other entities/objects have been instanciated;
	createPlayerEntity(entities, world);

	for (entityx::Entity entity : es_editor.entities_with_components<Render, Physics>()) {
		entities.create_from_copy(entity);
	}
}

void Game::clear() {
	// For all the entities that have been defined in the game, we delete them from the dynamic world
	World& world = Singleton<World>::getInstance();
	for (std::unordered_set<std::string>::iterator it = m_game_entity_names.begin(); it != m_game_entity_names.end(); it++) {
		const std::string& name = *it;
		world.deleteEntity(name);
	}
	m_game_entity_names.clear();
}



Game::Game(GameProgram& program, InputHandler& input_handler) : ProgramState(program, input_handler),
																	  m_theta(0.f),
																	  m_alpha(0.f),
																	  m_player_direction(0.f) {
	World& world = Singleton<World>::getInstance();
	// Init scripts
	initScripts();
	// Add game entities

	createGroundEntity(entities);
	createDoorEntity(entities, world);
	createSwordEntity("sword", entities, world);
	createSwordEntity("sword2", entities, world);

	// The player is instanciated in last after all other entities/objects have been instanciated;
	createPlayerEntity(entities, world);

	/// Set up systems
	systems.add<PhysicSystem>(entities, *(world.dynamic_world));
	systems.add<MovementSystem>();
	systems.add<ScriptSystem>(world.get("player"));
	systems.add<PickingSystem>(m_viewer, m_input_handler, world.get("player"));
	systems.add<RenderSystem>(m_viewer);

	systems.configure();

	// Keyboard callbacks definition
	Viewer& viewer = m_viewer;
	entityx::EntityManager& es = entities;
	entityx::EventManager& ev = events;

	m_commands.insert(std::pair<int, std::function<void()> >(SDLK_a, [&]() {
		createArrowEntity(entities, events);
	}));

	glm::vec3& direction_player = m_player_direction;

	m_commands.insert(std::pair<int, std::function<void()> >(SDLK_z, [&viewer, &direction_player]() {
		direction_player += glm::vec3(viewer.getDirection().x, 0.f, viewer.getDirection().z);
	}));
	m_commands.insert(std::pair<int, std::function<void()> >(SDLK_s, [&viewer, &direction_player]() {
		direction_player += glm::vec3(-viewer.getDirection().x, 0.f, -viewer.getDirection().z);
	}));
	m_commands.insert(std::pair<int, std::function<void()> >(SDLK_q, [&viewer, &direction_player]() {
		direction_player += glm::vec3(viewer.getDirection().z, 0.f, -viewer.getDirection().x);
	}));
	m_commands.insert(std::pair<int, std::function<void()> >(SDLK_d, [&viewer, &direction_player]() {
		direction_player += glm::vec3(-viewer.getDirection().z, 0.f, viewer.getDirection().x);
	}));

	m_commands.insert(std::pair<int, std::function<void()> >(SDLK_SPACE, [&viewer, &direction_player]() {
		direction_player += glm::vec3(0, 10.f, 0);
	}));

	/// Interaction Key : e 
	m_commands.insert(std::pair<int, std::function<void()> >(SDLK_e, [&ev, &world]() {
		ev.emit<InteractionEvent>({world.get("player")});
	}));
}

void Game::initScripts() const {
	Manager<std::string, std::shared_ptr<FiniteStateMachine>>& scripts = Manager<std::string, std::shared_ptr<FiniteStateMachine>>::getInstance();
	const Viewer& viewer = m_viewer;
	const InputHandler& input_handler = m_input_handler;

	/// Open door script
	FiniteStateMachine::State* door_opening = new FiniteStateMachine::State(
		[&viewer](entityx::Entity entity, entityx::Entity player) {
		entityx::ComponentHandle<Physics> physic = entity.component<Physics>();

		assert(physic->constraint != nullptr);
		assert(physic->constraint->getConstraintType() == btTypedConstraintType::HINGE_CONSTRAINT_TYPE);

		btHingeConstraint* constraint = (btHingeConstraint*)physic->constraint;
		
		btScalar angle = constraint->getHingeAngle();
		if (angle > constraint->getUpperLimit()*0.95f || angle < constraint->getLowerLimit()*0.95f) {
			physic->rigid_body->applyTorque(btVector3(0, 0, 0));
			physic->rigid_body->setAngularVelocity(btVector3(0, 0, 0));
			physic->rigid_body->setAngularFactor(btVector3(0, 0, 0));
			return;
		}

		physic->rigid_body->setAngularFactor(btVector3(0, 1, 0));
		btScalar start_angle = (constraint->getUpperLimit() + constraint->getLowerLimit()) / 2.f;
		
		glm::vec2 door_v(glm::cos(start_angle), -glm::sin(start_angle));
		glm::vec2 pos_v(viewer.getPosition().x, viewer.getPosition().z);
		pos_v = glm::normalize(pos_v);

		btScalar torque_y = -20;
		float sin_theta = pos_v.x * door_v.y - pos_v.y * door_v.x;
		if (sin_theta < 0.f)
			torque_y = -torque_y;

		physic->rigid_body->applyTorque(btVector3(0, torque_y, 0));
		std::cout << "Door opening ! grrrr" << std::endl;
	});

	/// Close door script
	FiniteStateMachine::State* door_closing = new FiniteStateMachine::State(
		[&viewer](entityx::Entity entity, entityx::Entity player) {
		entityx::ComponentHandle<Physics> physic = entity.component<Physics>();

		assert(physic->constraint != nullptr);
		assert(physic->constraint->getConstraintType() == btTypedConstraintType::HINGE_CONSTRAINT_TYPE);

		btHingeConstraint* constraint = (btHingeConstraint*)physic->constraint;

		btScalar angle = constraint->getHingeAngle();
		btScalar start_angle = (constraint->getUpperLimit() + constraint->getLowerLimit()) / 2.f;

		if (abs(angle - start_angle) < 0.05f) {
			physic->rigid_body->applyTorque(btVector3(0, 0, 0));
			physic->rigid_body->setAngularVelocity(btVector3(0, 0, 0));
			physic->rigid_body->setAngularFactor(btVector3(0, 0, 0));
			return;
		}
		physic->rigid_body->setAngularFactor(btVector3(0, 1, 0));
		
		bool open_upper_side = (angle > start_angle);

		btScalar torque_y = 20.f;
		if(open_upper_side)
			torque_y = -torque_y;

		physic->rigid_body->applyTorque(btVector3(0, torque_y, 0));
		std::cout << "Door closing ! grrrr" << std::endl;
	});
	
	door_opening->addTransition(FiniteStateMachine::Transition(door_closing,
		[&input_handler, &viewer](entityx::Entity entity, entityx::Entity player) {
		if (!input_handler.m_keydown)
			return false;
		if (input_handler.m_key.find(SDLK_e) == input_handler.m_key.end())
			return false;
		entityx::Entity entity_picked;
		if (!PickingSystem::isEntityPerInteraction(entity_picked, input_handler, viewer))
			return false;
		if (entity_picked != entity)
			return false;

		entityx::ComponentHandle<Physics> physic = entity.component<Physics>();

		assert(physic->constraint != nullptr);
		assert(physic->constraint->getConstraintType() == btTypedConstraintType::HINGE_CONSTRAINT_TYPE);

		btHingeConstraint* constraint = (btHingeConstraint*)physic->constraint;
		btScalar angle = constraint->getHingeAngle();
		if (angle < constraint->getUpperLimit()*0.95f && angle > constraint->getLowerLimit()*0.95f) {
			return false;
		}

		return true;
	}));

	door_closing->addTransition(FiniteStateMachine::Transition(door_opening,
		[&input_handler, &viewer](entityx::Entity entity, entityx::Entity player) {
		if (!input_handler.m_keydown)
			return false;
		if (input_handler.m_key.find(SDLK_e) == input_handler.m_key.end())
			return false;
		entityx::Entity entity_picked;
		if (!PickingSystem::isEntityPerInteraction(entity_picked, input_handler, viewer))
			return false;
		if (entity_picked != entity)
			return false;
		
		entityx::ComponentHandle<Physics> physic = entity.component<Physics>();

		assert(physic->constraint != nullptr);
		assert(physic->constraint->getConstraintType() == btTypedConstraintType::HINGE_CONSTRAINT_TYPE);

		btHingeConstraint* constraint = (btHingeConstraint*)physic->constraint;
		btScalar angle = constraint->getHingeAngle();
		btScalar start_angle = (constraint->getUpperLimit() + constraint->getLowerLimit()) / 2.f;

		if (abs(angle - start_angle) > 0.05f) {
			return false;
		}

		return true;
	}));

	/// Interaction script for picked objects
	FiniteStateMachine::State* picking = new FiniteStateMachine::State(
		[&viewer](entityx::Entity entity, entityx::Entity player) {
		entityx::ComponentHandle<Physics> physic = entity.component<Physics>();
		
	});

	scripts.insert("door_opening", std::make_shared<FiniteStateMachine>(door_opening));
}

Game::~Game() {
	World& world = Singleton<World>::getInstance();
	world.free();
}

void Game::addEntity(const std::string& name, entityx::Entity entity) {
	/*
	// Add it to the game's bullet dynamic world
	m_game_entities[name + std::to_string(id_entity)] = entity;

	entityx::ComponentHandle<Physics> physic = entity.component<Physics>();
	m_dynamic_world->addRigidBody(physic->rigid_body);
	id_entity++;
	*/
	m_game_entity_names.insert(name);
	World& world = Singleton<World>::getInstance();
	world.addEntity(name, entity);
}


void Game::run() {


	// Get a reference to the world
	World& world = Singleton<World>::getInstance();
	entityx::Entity player = world.get("player");


	GameProgram::m_current_viewer = &m_viewer;

	/// Systems updates
	//systems.update_all(1.f / 60.f);

	systems.update<PhysicSystem>(1.f / 60.f);
	systems.update<MovementSystem>(1.f / 60.f);
	systems.update<ScriptSystem>(1.f / 60.f);
	systems.update<PickingSystem>(1.f / 60.f);
	systems.update<RenderSystem>(1.f / 60.f);

	btTransform player_transform;
	player.component<Physics>()->motion_state->getWorldTransform(player_transform);
	glm::vec3 position(player_transform.getOrigin().x(), player_transform.getOrigin().y(), player_transform.getOrigin().z());
	m_viewer.setPosition(position);

	/// Player keyboard callbacks
	// Reset the direction vector of the player
	m_player_direction = glm::vec3(0.f);
	this->callbacks();

	/// Player view update
	// Viewer in game mode update
	// First Player Shooter
	if (m_input_handler.m_mouse_X >= 0 && m_input_handler.m_mouse_X <= GameProgram::width) {
		if (m_input_handler.m_mouse_Y >= 0 && m_input_handler.m_mouse_Y <= GameProgram::height) {
			float factorX = (float)m_input_handler.m_mouse_X / GameProgram::width - 0.5f;
			m_alpha += glm::abs(factorX) >= 0.1 ? factorX*0.1f : 0.f;
			float factorY = ((GameProgram::height - (float)m_input_handler.m_mouse_Y) / GameProgram::height - 0.5f);
			m_theta += glm::abs(factorY) >= 0.1f ? factorY*0.1f : 0.f;

			if (glm::abs(m_theta) >= 3.14f / 3.f) {
				m_theta = glm::sign(m_theta)*3.14f / 3.f;
			}

			m_viewer.setDirection(glm::vec3(glm::sin(m_alpha)*glm::cos(m_theta),
				glm::sin(m_theta),
				-glm::cos(m_alpha)*glm::cos(m_theta)));
			
			btTransform tr = player.component<Physics>()->rigid_body->getWorldTransform();
			//tr.setIdentity();
			tr.setRotation(btQuaternion(-m_alpha, 0, 0));
			player.component<Physics>()->rigid_body->setWorldTransform(tr);

		}
	}

	// Send an DisplacementEvent of the player entity to the MovementSystem.
	if (m_player_direction != glm::vec3(0.f)) {
		// Normalization of the direction of the player
		glm::normalize(m_player_direction);
		events.emit<DisplacementEvent>(player, m_player_direction);
	}
	else {
		events.emit<StopDisplacementEvent>(player);
	}

	/// Set viewer on the player position
	entityx::ComponentHandle<Physics> physic = player.component<Physics>();
	//btVector3 viewer_pos(m_viewer.getPosition().x, m_viewer.getPosition().y, m_viewer.getPosition().z);
	//btVector3 pos_player = viewer_pos + m_player_direction;
	//std::cout << physic->rigid_body->getLinearVelocity().x() << " " << physic->rigid_body->getLinearVelocity().y() << " " << physic->rigid_body->getLinearVelocity().z() << std::endl;
	

}