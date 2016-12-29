// Copyright 2014-2016 the openage authors. See copying.md for legal info.

#pragma once

#include <functional>
#include <map>
#include <algorithm>

#include "../coord/tile.h"
#include "../gamedata/unit.gen.h"
#include "../terrain/terrain_object.h"
#include "../gamestate/resource.h"
#include "unit_container.h"

namespace std {

/**
 * hasher for unit classes enum type
 */
template<> struct hash<gamedata::unit_classes> {
	typedef underlying_type<gamedata::unit_classes>::type underlying_type;
	typedef hash<underlying_type>::result_type result_type;
	result_type operator()(const gamedata::unit_classes &arg) const {
		hash<underlying_type> hasher;
		return hasher(static_cast<underlying_type>(arg));
	}
};

} // namespace std

namespace openage {

/**
 * types of action graphics
 */
enum class graphic_type {
	construct,
	shadow,
	decay,
	dying,
	standing,
	walking,
	carrying,
	attack,
	heal,
	work
};

class UnitTexture;

/**
 * collection of graphics attached to each unit
 */
using graphic_set = std::map<graphic_type, std::shared_ptr<UnitTexture>>;

/**
 * list of attribute types
 */
enum class attr_type {
	owner,
	damaged,
	hitpoints,
	armor,
	attack,
	heal,
	speed,
	direction,
	projectile,
	building,
	dropsite,
	resource,
	gatherer,
	garrison
};

enum class attack_stance {
	aggresive,
	devensive,
	stand_ground,
	do_nothing
};

/**
 * this type gets specialized for each attribute
 */
template<attr_type T> class Attribute;

/**
 * wraps a templated attribute
 */
class AttributeContainer {
public:
	AttributeContainer() {}

	AttributeContainer(attr_type t)
		:
		type{t} {}

	attr_type type;

	/**
	 * shared attributes are common across all units of
	 * one type, such as max hp, and gather rates
	 *
	 * non shared attributes include a units current hp,
	 * and the amount a villager is carrying
	 */
	virtual bool shared() const = 0;

	/**
	 * Produces an copy of the attribute.
	 */
	virtual std::shared_ptr<AttributeContainer> copy() const = 0;
};

/**
 * Contains a group of attributes.
 * Can contain only one attribute of each type.
 */
class Attributes {
public:
	Attributes() {}

	/**
	 * Add an attribute or replace any attribute of the same type.
	 */
	void add(const std::shared_ptr<AttributeContainer> attr);

	/**
	 * Add copies of all the attributes from the given Attributes.
	 */
	void addCopies(const Attributes &attrs);

	/**
	 * Remove an attribute based on the type.
	 */
	bool remove(const attr_type type);

	/**
	 * Check if the attribute of the given type exists.
	 */
	bool has(const attr_type type) const;

	/**
	 * Get the attribute based on the type.
	 */
	std::shared_ptr<AttributeContainer> get(const attr_type type) const;

	/**
	 * Get the attribute
	 */
	template<attr_type T>
	Attribute<T> &get() const;

private:

	std::map<attr_type, std::shared_ptr<AttributeContainer>> attrs;
};

/**
 * An unordered_map with a int key used as a type id
 * and a unsigned int value used as the amount
 */
using typeamount_map = std::unordered_map<int, unsigned int>;

/**
 * Wraps a templated shared attribute
 *
 * Shared attributes are common across all units of
 * one type
 */
class SharedAttributeContainer: public AttributeContainer {
public:

	SharedAttributeContainer(attr_type t)
		:
		AttributeContainer{t} {}

	bool shared() const override {
		return true;
	}

};

/**
 * Wraps a templated unshared attribute
 *
 * Shared attributes are copied for each unit of
 * one type
 */
class UnsharedAttributeContainer: public AttributeContainer {
public:

	UnsharedAttributeContainer(attr_type t)
		:
		AttributeContainer{t} {}

	bool shared() const override {
		return false;
	}

};

// -----------------------------
// attribute definitions go here
// -----------------------------

class Player;

template<> class Attribute<attr_type::owner>: public SharedAttributeContainer {
public:
	Attribute(Player &p)
		:
		SharedAttributeContainer{attr_type::owner},
		player(p) {}

	std::shared_ptr<AttributeContainer> copy() const override {
		return std::make_shared<Attribute<attr_type::owner>>(*this);
	}

	Player &player;
};

/**
 * The max hitpoints and health bar information.
 * TODO change bar information stucture
 */
template<> class Attribute<attr_type::hitpoints>: public UnsharedAttributeContainer {
public:
	Attribute(unsigned int i)
		:
		UnsharedAttributeContainer{attr_type::hitpoints},
		hp{i} {}

	std::shared_ptr<AttributeContainer> copy() const override {
		return std::make_shared<Attribute<attr_type::hitpoints>>(*this);
	}

	/**
	 * The max hitpoints
	 */
	unsigned int hp;
	float hp_bar_height;
};

/**
 * The current hitpoints.
 * TODO add last damage taken timestamp
 */
template<> class Attribute<attr_type::damaged>: public UnsharedAttributeContainer {
public:
	Attribute(unsigned int i)
		:
		UnsharedAttributeContainer{attr_type::damaged},
		hp{i} {}

	std::shared_ptr<AttributeContainer> copy() const override {
		return std::make_shared<Attribute<attr_type::damaged>>(*this);
	}

	/**
	 * The current hitpoint
	 */
	unsigned int hp;
};

template<> class Attribute<attr_type::armor>: public SharedAttributeContainer {
public:
	Attribute(typeamount_map a)
		:
		SharedAttributeContainer{attr_type::armor},
		armor{a} {}

	std::shared_ptr<AttributeContainer> copy() const override {
		return std::make_shared<Attribute<attr_type::armor>>(*this);
	}

	typeamount_map armor;
};

template<> class Attribute<attr_type::attack>: public UnsharedAttributeContainer {
public:
	// TODO remove (keep for testing)
	// 4 = gamedata::hit_class::UNITS_MELEE (not exported at the moment)
	Attribute(UnitType *type, coord::phys_t r, coord::phys_t h, unsigned int d, UnitType *reset_type)
		:
		Attribute{type, r, h, {{4, d}}, reset_type} {}

	Attribute(UnitType *type, coord::phys_t r, coord::phys_t h, typeamount_map d, UnitType *reset_type)
		:
		UnsharedAttributeContainer{attr_type::attack},
		ptype{type},
		range{r},
		init_height{h},
		damage{d},
		stance{attack_stance::do_nothing},
		attack_type{reset_type} {}

	std::shared_ptr<AttributeContainer> copy() const override {
		return std::make_shared<Attribute<attr_type::attack>>(*this);
	}

	// TODO: can a unit have multiple attacks such as villagers hunting
	// map target classes onto attacks

	UnitType *ptype; // projectile type
	coord::phys_t range;
	coord::phys_t init_height;
	typeamount_map damage;
	attack_stance stance;

	// TODO move elsewhere in order to become shared attribute
	// used to change graphics back to normal for villagers
	UnitType *attack_type;
};

template<> class Attribute<attr_type::heal>: public SharedAttributeContainer {
public:
	Attribute(coord::phys_t r, coord::phys_t h, unsigned int l, float ra)
		:
		SharedAttributeContainer{attr_type::heal},
		range{r},
		init_height{h},
		life{l},
		rate{ra} {}

	std::shared_ptr<AttributeContainer> copy() const override {
		return std::make_shared<Attribute<attr_type::heal>>(*this);
	}

	coord::phys_t range;
	coord::phys_t init_height; // TODO remove?
	unsigned int life;
	float rate;
};

template<> class Attribute<attr_type::speed>: public SharedAttributeContainer {
public:
	Attribute(coord::phys_t sp)
		:
		SharedAttributeContainer{attr_type::speed},
		unit_speed{sp} {}

	std::shared_ptr<AttributeContainer> copy() const override {
		return std::make_shared<Attribute<attr_type::speed>>(*this);
	}

	coord::phys_t unit_speed; // possibly use a pointer to account for tech upgrades
};

template<> class Attribute<attr_type::direction>: public UnsharedAttributeContainer {
public:
	Attribute(coord::phys3_delta dir)
		:
		UnsharedAttributeContainer{attr_type::direction},
		unit_dir(dir) {}

	std::shared_ptr<AttributeContainer> copy() const override {
		return std::make_shared<Attribute<attr_type::direction>>(*this);
	}

	coord::phys3_delta unit_dir;
};

template<> class Attribute<attr_type::projectile>: public UnsharedAttributeContainer {
public:
	Attribute(float arc)
		:
		UnsharedAttributeContainer{attr_type::projectile},
		projectile_arc{arc},
		launched{false} {}

	std::shared_ptr<AttributeContainer> copy() const override {
		return std::make_shared<Attribute<attr_type::projectile>>(*this);
	}

	float projectile_arc;
	UnitReference launcher;
	bool launched;
};

template<> class Attribute<attr_type::building>: public UnsharedAttributeContainer {
public:
	Attribute()
		:
		UnsharedAttributeContainer{attr_type::building},
		completed{.0f},
		is_dropsite{true},
		foundation_terrain{0} {}

	bool shared() const override {
		return false;
	}

	std::shared_ptr<AttributeContainer> copy() const override {
		return std::make_shared<Attribute<attr_type::building>>(*this);
	}

	float completed;
	bool is_dropsite;
	int foundation_terrain;

	// set the TerrainObject to this state
	// once building has been completed
	object_state completion_state;

	// TODO: use unit class, fish and forage have different dropsites
	game_resource resource_type;

	// TODO: list allowed trainable producers
	UnitType *pp;
	coord::phys3 gather_point;
};

template<> class Attribute<attr_type::dropsite>: public SharedAttributeContainer {
public:

	Attribute(std::vector<game_resource> types)
		:
		SharedAttributeContainer{attr_type::dropsite},
		resource_types{types} {}

	std::shared_ptr<AttributeContainer> copy() const override {
		return std::make_shared<Attribute<attr_type::dropsite>>(*this);
	}

	bool accepting_resource(game_resource res) {
		if (std::find(resource_types.begin(), resource_types.end(), res) != resource_types.end()) {
			return true;
		} else {
			return false;
		}
	}

private:
	std::vector<game_resource> resource_types;
};

/**
 * resource capacity of an object, trees, mines, villagers etc.
 */
template<> class Attribute<attr_type::resource>: public UnsharedAttributeContainer {
public:
	Attribute(game_resource type, float init_amount)
		:
		UnsharedAttributeContainer{attr_type::resource},
		resource_type{type},
		amount{init_amount} {}

	std::shared_ptr<AttributeContainer> copy() const override {
		return std::make_shared<Attribute<attr_type::resource>>(*this);
	}

	game_resource resource_type;
	float amount;
};

class UnitTexture;

/**
 * TODO: rename to worker
 */
template<> class Attribute<attr_type::gatherer>: public UnsharedAttributeContainer {
public:
	Attribute()
		:
		UnsharedAttributeContainer{attr_type::gatherer},
		amount{.0f} {}

	std::shared_ptr<AttributeContainer> copy() const override {
		return std::make_shared<Attribute<attr_type::gatherer>>(*this);
	}

	game_resource current_type;
	float amount;
	float capacity;
	float gather_rate;

	// texture sets available for each resource
	std::unordered_map<gamedata::unit_classes, UnitType *> graphics;
};

template<> class Attribute<attr_type::garrison>: public UnsharedAttributeContainer {
public:
	Attribute()
		:
		UnsharedAttributeContainer{attr_type::garrison} {}

	std::shared_ptr<AttributeContainer> copy() const override {
		return std::make_shared<Attribute<attr_type::garrison>>(*this);
	}

	std::vector<UnitReference> content;
};

} // namespace openage
