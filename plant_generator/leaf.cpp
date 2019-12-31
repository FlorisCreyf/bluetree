/* Copyright 2017 Floris Creyf
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "leaf.h"

using pg::Leaf;

long Leaf::counter = 1;

Leaf::Leaf()
{
	id = counter++;
	position = -1.0f;
	scale = {1.0f, 1.0f, 1.0f};
	material = 0;
	mesh = 0;
	rotation = {0.0f, 0.0f, 0.0f, 1.0f};
}

bool Leaf::operator==(const Leaf &leaf) const
{
	return (
		id == leaf.id &&
		position == leaf.position &&
		scale == leaf.scale &&
		material == leaf.material &&
		rotation == leaf.rotation &&
		mesh == leaf.mesh
	);
}

bool Leaf::operator!=(const Leaf &leaf) const
{
	return !(*this == leaf);
}

long Leaf::getID() const
{
	return id;
}

void Leaf::setPosition(float position)
{
	this->position = position;
}

float Leaf::getPosition() const
{
	return position;
}

void Leaf::setRotation(pg::Quat rotation)
{
	this->rotation = rotation;
}

pg::Quat Leaf::getRotation()
{
	return rotation;
}

pg::Quat Leaf::getDefaultOrientation(pg::Vec3 stemDirection)
{
	Vec3 normal = {0.0f, 1.0f, 0.0f};
	Vec3 d = {0.0f, 0.0f, 1.0f};
	Vec3 leafDirection = normalize(pg::cross(stemDirection, normal));
	Quat q = pg::rotateIntoVecQ(d, leafDirection);
	Vec3 up = {0.0f, -1.0f, 0.0f};
	d = pg::cross(up, stemDirection);
	d = pg::cross(d, stemDirection);
	d = normalize(d);
	Quat k = pg::rotateIntoVecQ(normal, d);
	return k * q;
}

void Leaf::setScale(pg::Vec3 scale)
{
	this->scale = scale;
}

pg::Vec3 Leaf::getScale() const
{
	return scale;
}

void Leaf::setMaterial(long material)
{
	this->material = material;
}

long Leaf::getMaterial() const
{
	return material;
}

void Leaf::setMesh(long mesh)
{
	this->mesh = mesh;
}

long Leaf::getMesh() const
{
	return mesh;
}