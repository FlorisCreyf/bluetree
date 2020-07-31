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

#include "mesh.h"
#include <cmath>
#include <limits>

#define PI 3.14159265359f

using namespace pg;
using std::map;
using std::vector;

Mesh::Mesh(Plant *plant)
{
	this->plant = plant;
	this->defaultLeaf.setPlane();
}

void Mesh::generate()
{
	Stem *stem = this->plant->getRoot();
	initBuffer();
	if (stem) {
		State state = {};
		addStem(stem, state);
		updateSegments();
	}
}

bool Mesh::hasValidLocation(Stem *stem)
{
	return !std::isnan(stem->getLocation().x);
}

Segment Mesh::addStem(Stem *stem, const State &parentState)
{
	State state;
	state.mesh = selectBuffer(stem->getMaterial(Stem::Outer));
	state.segment.stem = stem;
	state.segment.vertexStart = this->vertices[state.mesh].size();
	state.segment.indexStart = this->indices[state.mesh].size();
	setInitialJointState(state, parentState);
	addSections(state);
	state.segment.vertexCount = this->vertices[state.mesh].size();
	state.segment.vertexCount -= state.segment.vertexStart;
	state.segment.indexCount = this->indices[state.mesh].size();
	state.segment.indexCount -= state.segment.indexStart;
	this->stemSegments[state.mesh].emplace(stem, state.segment);

	addLeaves(stem, state);

	Stem *child = stem->getChild();
	while (child != nullptr) {
		if (hasValidLocation(child))
			addStem(child, state);
		child = child->getSibling();
	}

	return state.segment;
}

void Mesh::addSections(State &state)
{
	Stem *stem = state.segment.stem;

	setInitialRotation(state);
	if (stem->getSectionDivisions() != this->crossSection.getResolution())
		this->crossSection.generate(stem->getSectionDivisions());

	state.texOffset = 0.0f;
	state.prevIndex = this->vertices[state.mesh].size();
	state.section = createBranchCollar(state);
	size_t sections = stem->getPath().getSize();

	if (state.section > 0 && state.section < sections) {
		size_t i = this->vertices[state.mesh].size();
		int r = stem->getSectionDivisions();
		addTriangleRing(state.prevIndex, i, r, state.mesh);
	}

	for (; state.section < sections; state.section++) {
		Quat rotation = rotateSection(state);
		state.prevIndex = this->vertices[state.mesh].size();
		addSection(state, rotation, this->crossSection);

		if (state.section+1 < sections) {
			size_t i = this->vertices[state.mesh].size();
			int r = stem->getSectionDivisions();
			addTriangleRing(state.prevIndex, i, r, state.mesh);
		}
	}

	if (stem->getMinRadius() > 0)
		capStem(stem, state.mesh, state.prevIndex);
}

/** The cross section is rotated so that the first point is always the topmost
point relative to the parent stem direction. */
void Mesh::setInitialRotation(State &state)
{
	Stem *stem = state.segment.stem;
	Stem *parent = stem->getParent();
	if (parent) {
		float position = stem->getDistance();
		Path parentPath = parent->getPath();
		Vec3 parentDirection;
		parentDirection = parentPath.getIntermediateDirection(position);
		Vec3 stemDirection = stem->getPath().getDirection(0);
		Vec3 up(0.0f, 1.0f, 0.0f);
		state.prevRotation = pg::rotateIntoVecQ(up, stemDirection);
		state.prevDirection = stemDirection;

		Vec3 sideways(1.0f, 0.0f, 0.0f);
		sideways = pg::rotate(state.prevRotation, sideways, 0.0f);
		sideways = pg::normalize(sideways);
		up = pg::projectOntoPlane(parentDirection, stemDirection);
		up = pg::normalize(up);
		Quat rotation = pg::rotateIntoVecQ(sideways, up);
		state.prevRotation = rotation * state.prevRotation;
	} else {
		state.prevRotation = Quat(0.0f, 0.0f, 0.0f, 1.0f);
		state.prevDirection = Vec3(0.0f, 1.0f, 0.0f);
	}
}

/** A rotation for a cross section is relative to the rotation of the previous
cross section. Twisting can occur in stems if all sections are rotated relative
to the global axis. */
Quat Mesh::rotateSection(State &state)
{
	Stem *stem = state.segment.stem;
	Vec3 direction = stem->getPath().getAverageDirection(state.section);
	Quat rotation = rotateIntoVecQ(state.prevDirection, direction);
	rotation *= state.prevRotation;
	state.prevRotation = rotation;
	state.prevDirection = direction;
	return rotation;
}

/** Generate a cross section for a point in the stem's path. Indices are added
at a latter stage to connect the sections. */
void Mesh::addSection(State &state, Quat rotation, const CrossSection &section)
{
	Stem *stem = state.segment.stem;

	DVertex vertex;
	vertex.uv.y = getTextureLength(stem, state.section) + state.texOffset;
	state.texOffset = vertex.uv.y;

	Vec3 location = stem->getLocation();
	location += stem->getPath().get(state.section);

	Vec2 indices;
	Vec2 weights;
	if (stem->getJoints().size() > 0)
		updateJointState(state, indices, weights);
	else {
		indices = Vec2(state.jointID, 0.0f);
		weights = Vec2(1.0f, 0.0f);
	}

	float radius = this->plant->getRadius(stem, state.section);
	const std::vector<SVertex> sectionVertices = section.getVertices();
	for (size_t i = 0; i < sectionVertices.size(); i++) {
		vertex.position = sectionVertices[i].position;
		vertex.normal = sectionVertices[i].normal;
		vertex.uv.x = sectionVertices[i].uv.x;

		vertex.position = radius * vertex.position;
		vertex.position = rotate(rotation, vertex.position, 1.0f);
		vertex.position += location;
		vertex.normal = rotate(rotation, vertex.normal, 0.0f);
		vertex.normal = normalize(vertex.normal);
		vertex.weights = weights;
		vertex.indices = indices;

		this->vertices[state.mesh].push_back(vertex);
	}
}

float getAspect(const Plant *plant, const Stem *stem)
{
	float aspect = 1.0f;
	long materialID = stem->getMaterial(Stem::Outer);
	if (materialID > 0) {
		Material m = plant->getMaterial(materialID);
		aspect = m.getRatio();
	}
	return aspect;
}

/** Determine a length to preserve the aspect ratio throughout the stem. */
float Mesh::getTextureLength(Stem *stem, size_t section)
{
	if (section > 0) {
		float length = stem->getPath().getSegmentLength(section);
		float radius = this->plant->getRadius(stem, section-1);
		float aspect = getAspect(this->plant, stem);
		return (length * aspect) / (radius * 2.0f * PI);
	} else
		return 0.0f;
}

/** Compute indices between the cross section just generated (starting at the
previous index) and the cross-section that still needs to be generated
(starting at the current index). */
void Mesh::addTriangleRing(
	size_t prevIndex, size_t index, int divisions, int mesh)
{
	for (int i = 0; i <= divisions - 1; i++) {
		addTriangle(mesh, index, index + 1, prevIndex);
		index++;
		addTriangle(mesh, prevIndex, index, prevIndex + 1);
		prevIndex++;
	}
}

size_t Mesh::createBranchCollar(State &state)
{
	Stem *stem = state.segment.stem;

	Vec2 swelling = stem->getSwelling();
	if (!stem->getParent() || swelling.x < 1.0f || swelling.y < 1.0f)
		return 0;

	state.section = 0;
	state.prevIndex = this->vertices[state.mesh].size();
	addSection(state, rotateSection(state), this->crossSection);

	size_t collarStart = this->vertices[state.mesh].size();
	reserveBranchCollarSpace(stem, state.mesh);
	state.prevIndex = this->vertices[state.mesh].size();
	state.texOffset = 0.0f;
	state.section = stem->getPath().getDivisions() + 1;
	state.prevIndex = this->vertices[state.mesh].size();
	addSection(state, rotateSection(state), this->crossSection);

	Segment parentSegment = findStem(stem->getParent());
	return connectCollar(state.segment, parentSegment, collarStart);
}

/** Cross sections are usually created one at a time and then connected with
triangles. Branch collars are created by connecting cross sections with
splines, which means that many cross sections are created at a time. Reserving
memory in advance enables offsets to be used to maintain an identical vertex
layout. */
void Mesh::reserveBranchCollarSpace(Stem *stem, int mesh)
{
	size_t size = getBranchCollarSize(stem) + this->vertices[mesh].size();
	this->vertices[mesh].resize(size);
}

/** Return the amount of memory needed for the branch collar. */
size_t Mesh::getBranchCollarSize(Stem *stem)
{
	return (stem->getSectionDivisions()+1) * stem->getCollarDivisions();
}

/** The first step in generating the branch collar is scaling the first cross
section of the stem. This method returns the quantity to scale by. */
Mat4 Mesh::getBranchCollarScale(Stem *child, Stem *parent)
{
	float position = child->getDistance();
	Vec3 yaxis = parent->getPath().getIntermediateDirection(position);
	Vec3 xaxis = child->getPath().getDirection(0);
	xaxis = normalize(cross(cross(yaxis, xaxis), yaxis));
	Vec3 zaxis = normalize(cross(yaxis, xaxis));

	Mat4 axes = identity();
	axes.vectors[0] = toVec4(xaxis, 0.0f);
	axes.vectors[1] = toVec4(yaxis, 0.0f);
	axes.vectors[2] = toVec4(zaxis, 0.0f);

	Mat4 scale = identity();
	scale[2][2] = child->getSwelling().x;
	scale[1][1] = child->getSwelling().y;

	return axes * scale * transpose(axes);
}

/** Project a point from a cross section on its parent's surface. */
DVertex Mesh::moveToSurface(DVertex vertex, Ray ray, Segment parent, int mesh)
{
	float length = magnitude(ray.direction);
	ray.direction = normalize(ray.direction);

	float t = std::numeric_limits<float>::max();
	unsigned i = parent.indexStart;
	while (i < parent.indexStart + parent.indexCount) {
		unsigned index;
		index = this->indices[mesh][i++];
		Vec3 p1 = this->vertices[mesh][index].position;
		index = this->indices[mesh][i++];
		Vec3 p2 = this->vertices[mesh][index].position;
		index = this->indices[mesh][i++];
		Vec3 p3 = this->vertices[mesh][index].position;

		float s = -intersectsTriangle(ray, p1, p3, p2);
		if (s != 0 && s < t) {
			t = s;
			vertex.normal = cross(p1-p2, p1-p3);
		}
	}

	if (t != std::numeric_limits<float>::max()) {
		Vec3 offset = (t - length) * ray.direction;
		vertex.normal = normalize(vertex.normal);
		vertex.position -= offset;
	} else {
		vertex.position.x = std::numeric_limits<float>::infinity();
		vertex.position.y = std::numeric_limits<float>::infinity();
		vertex.position.z = std::numeric_limits<float>::infinity();
	}

	return vertex;
}

size_t Mesh::connectCollar(Segment child, Segment parent, size_t vertexStart)
{
	const int mesh1 = selectBuffer(child.stem->getMaterial(Stem::Outer));
	const int mesh2 = selectBuffer(parent.stem->getMaterial(Stem::Outer));
	const int sectionDivisions = child.stem->getSectionDivisions();
	const int collarDivisions = child.stem->getCollarDivisions();
	const int pathDivisions = child.stem->getPath().getDivisions();
	size_t collarSize = getBranchCollarSize(child.stem);
	Mat4 scale = getBranchCollarScale(child.stem, parent.stem);

	int degree;
	Vec3 direction;
	Spline spline = child.stem->getPath().getSpline();
	degree = spline.getDegree();
	if (degree == 3) {
		auto controls = spline.getControls();
		direction = controls[3] - controls[2];
	}

	for (int i = 0; i <= sectionDivisions; i++) {
		size_t index = child.vertexStart + i;
		size_t nextIndex = index + collarSize + sectionDivisions + 1;

		Ray ray;
		Vec3 location = child.stem->getLocation();
		DVertex initPoint = this->vertices[mesh1][index];
		DVertex scaledPoint;

		scaledPoint.position = initPoint.position - location;
		scaledPoint.position = scale.apply(scaledPoint.position, 1.0f);
		scaledPoint.position += location;
		ray.origin = this->vertices[mesh1][nextIndex].position;
		ray.direction = ray.origin - scaledPoint.position;
		scaledPoint = moveToSurface(scaledPoint, ray, parent, mesh2);
		if (std::isinf(scaledPoint.position.x)) {
			this->vertices[mesh1].resize(child.vertexStart);
			this->indices[mesh1].resize(child.indexStart);
			return 0;
		}
		scaledPoint.weights = this->vertices[mesh1][index].weights;
		scaledPoint.indices = this->vertices[mesh1][index].indices;
		this->vertices[mesh1][index] = scaledPoint;

		ray.direction = ray.origin - initPoint.position;
		initPoint = moveToSurface(initPoint, ray, parent, mesh2);
		if (std::isinf(initPoint.position.x)) {
			this->vertices[mesh1].resize(child.vertexStart);
			this->indices[mesh1].resize(child.indexStart);
			return 0;
		}

		spline.clear();
		spline.setDegree(3);
		spline.addControl(scaledPoint.position);
		spline.addControl(initPoint.position);
		Vec3 point = this->vertices[mesh1][nextIndex].position;
		if (degree == 3)
			spline.addControl(point - direction);
		else
			spline.addControl(point);
		spline.addControl(point);

		float delta = 1.0f/(collarDivisions+1);
		float t = delta;
		index = vertexStart + i;
		for (int j = 0; j < collarDivisions; j++, t += delta) {
			DVertex vertex;
			vertex.position = spline.getPoint(0, t);
			vertex.indices = scaledPoint.indices;
			vertex.weights = scaledPoint.weights;
			size_t offset = index + (sectionDivisions + 1) * j;
			this->vertices[mesh1][offset] = vertex;
		}
	}

	size_t index1 = child.vertexStart;
	size_t index2 = child.vertexStart + sectionDivisions + 1;
	for (int i = 0; i <= collarDivisions; i++) {
		addTriangleRing(index1, index2, sectionDivisions, mesh1);
		index1 = index2;
		index2 += sectionDivisions + 1;
	}

	index1 = child.vertexStart;
	index2 = vertexStart + collarSize;
	setBranchCollarNormals(
		index1, index2, mesh1, sectionDivisions, collarDivisions);
	setBranchCollarUVs(
		index2, child.stem, mesh1, sectionDivisions, collarDivisions);
	return pathDivisions + 2;
}

/** Interpolate normals from the first cross section of the stem with normals
from the first cross section after the branch collar. */
void Mesh::setBranchCollarNormals(
	size_t index1, size_t index2, int mesh, int resolution, int divisions)
{
	for (int i = 0; i <= resolution; i++) {
		Vec3 normal1 = this->vertices[mesh][index1].normal;
		Vec3 normal2 = this->vertices[mesh][index2].normal;

		for (int j = 1; j <= divisions; j++) {
			float t = j/static_cast<float>(divisions);
			Vec3 normal = lerp(normal1, normal2, t);
			normal = normalize(normal);
			size_t offset = j * (resolution + 1);
			this->vertices[mesh][index1 + offset].normal = normal;
		}

		index1++;
		index2++;
	}
}

/** Normally UV coordinates are generated starting at the first cross section.
The UV coordinates for branch collars are generated backwards because splines
are not guaranteed to be the same length. */
void Mesh::setBranchCollarUVs(
	size_t lastIndex, Stem *stem, int mesh, int resolution, int divisions)
{
	size_t size = resolution + 1;
	float radius = this->plant->getRadius(stem, 1);
	float aspect = getAspect(this->plant, stem);

	for (int i = 0; i <= resolution; i++) {
		Vec2 uv = this->vertices[mesh][lastIndex + i].uv;
		size_t index = lastIndex + i;

		for (int j = divisions; j >= 0; j--) {
			Vec3 p1 = this->vertices[mesh][index].position;
			index -= size;
			Vec3 p2 = this->vertices[mesh][index].position;
			float length = magnitude(p2 - p1);
			uv.y -= (length * aspect) / (radius * 2.0f * PI);
			this->vertices[mesh][index].uv = uv;
		}
	}
}

void Mesh::capStem(Stem *stem, int stemMesh, size_t section)
{
	int mesh = selectBuffer(stem->getMaterial(Stem::Inner));
	int index = section;
	int divisions = stem->getSectionDivisions();
	float rotation = 2.0f * PI / divisions;
	float angle = 0.0f;
	section = this->vertices[mesh].size();

	for (int i = 0; i <= divisions; i++, index++, angle += rotation) {
		DVertex vertex = this->vertices[stemMesh][index];
		vertex.uv.x = std::cos(angle) * 0.5f + 0.5f;
		vertex.uv.y = std::sin(angle) * 0.5f + 0.5f;
		this->vertices[mesh].push_back(vertex);
	}

	for (index = 0; index < divisions/2 - 1; index++) {
		addTriangle(mesh,
			section + index,
			section + divisions - index - 1,
			section + index + 1);
		addTriangle(mesh,
			section + index + 1,
			section + divisions - index - 1,
			section + divisions - index - 2);
	}

	if ((divisions & 1) != 0) { /* is odd */
		size_t lastSection = section + index;
		addTriangle(mesh, lastSection, lastSection+2, lastSection+1);
	}
}

void Mesh::addLeaves(Stem *stem, const State &state)
{
	auto leaves = stem->getLeaves();
	unsigned index = 0;
	for (auto it = leaves.begin(); it != leaves.end(); it++)
		addLeaf(stem, index++, state);
}

void Mesh::addLeaf(Stem *stem, unsigned leafIndex, const State &state)
{
	Leaf *leaf = stem->getLeaf(leafIndex);
	int mesh = selectBuffer(leaf->getMaterial());
	Segment leafSegment;
	leafSegment.leafIndex = leafIndex;
	leafSegment.stem = stem;
	leafSegment.vertexStart = this->vertices[mesh].size();
	leafSegment.indexStart = this->indices[mesh].size();

	Vec2 weights;
	Vec2 indices;
	if (stem->hasJoints()) {
		float position = leaf->getPosition();
		auto pair = getJoint(position, stem);
		size_t index = pair.first;
		size_t pathIndex = pair.second.getPathIndex();
		float jointPosition = stem->getPath().getDistance(pathIndex);
		float offset = position - jointPosition;
		setJointInfo(stem, offset, index, weights, indices);
	} else {
		weights.x = 1.0f;
		weights.y = 0.0f;
		indices.x = static_cast<float>(state.jointID);
		indices.y = indices.x;
	}

	Geometry geom = transformLeaf(leaf, stem);
	size_t vsize = this->vertices[mesh].size();
	for (DVertex vertex : geom.getPoints()) {
		vertex.indices = indices;
		vertex.weights = weights;
		this->vertices[mesh].push_back(vertex);
	}
	for (unsigned i : geom.getIndices())
		this->indices[mesh].push_back(i + vsize);

	leafSegment.vertexCount = this->vertices[mesh].size();
	leafSegment.vertexCount -= leafSegment.vertexStart;
	leafSegment.indexCount = this->indices[mesh].size();
	leafSegment.indexCount -= leafSegment.indexStart;
	this->leafSegments[mesh].emplace(LeafID(stem, leafIndex), leafSegment);
}

Geometry Mesh::transformLeaf(const Leaf *leaf, const Stem *stem)
{
	const Path path = stem->getPath();
	Vec3 location = stem->getLocation();
	float position = leaf->getPosition();
	Vec3 direction;

	if (position >= 0.0f && position < path.getLength()) {
		direction = path.getIntermediateDirection(position);
		location += path.getIntermediate(position);
	} else {
		direction = path.getDirection(path.getSize() - 1);
		location += path.get().back();
	}

	Geometry geom = this->plant->getLeafMesh(leaf->getMesh());
	Quat rotation;
	rotation = leaf->getDefaultOrientation(direction);
	rotation *= leaf->getRotation();
	geom.transform(rotation, leaf->getScale(), location);
	return geom;
}

/** Stem descendants might not have joints and the parent state is needed to
determine what joint ancestors are influenced by. */
void Mesh::setInitialJointState(State &state, const State &parentState)
{
	const Stem *stem = state.segment.stem;
	const Stem *parent = stem->getParent();
	state.jointID = 0;
	state.jointIndex = 0;
	state.jointOffset = 0.0f;
	const vector<Joint> joints = stem->getJoints();

	if (joints.empty() && (!parent || !parent->hasJoints())) {
		state.jointID = parentState.jointID;
	} else if (joints.empty()) {
		float position = stem->getDistance();
		auto pair = getJoint(position, stem->getParent());
		state.jointID = pair.second.getID();
		state.jointIndex = pair.first;
	} else {
		Joint joint = joints.front();
		state.jointID = joint.getID();
	}
}

std::pair<size_t, Joint> Mesh::getJoint(float position, const Stem *stem)
{
	size_t index = stem->getPath().getIndex(position);
	const vector<Joint> joints = stem->getJoints();
	size_t jointIndex = 0;
	for (auto it = joints.begin(); it != joints.end(); it++) {
		size_t pathIndex = it->getPathIndex();
		if (pathIndex > index) {
			if (it != joints.begin()) {
				jointIndex--;
				it--;
			}
			return std::pair<size_t, Joint>(jointIndex, *it);
		}
		jointIndex++;
	}
	return std::pair<size_t, Joint>(jointIndex-1, joints.back());
}

void Mesh::incrementJoint(State &state, const vector<Joint> &joints)
{
	if (state.jointIndex + 1 < joints.size()) {
		Joint nextJoint = joints[state.jointIndex + 1];
		if (nextJoint.getPathIndex() == state.section) {
			state.jointIndex++;
			state.jointID = nextJoint.getID();
			state.jointOffset = 0.0f;
		}
	}
}

/** Update the current joint and set the joint indices and weights. */
void Mesh::updateJointState(State &state, Vec2 &indices, Vec2 &weights)
{
	const Stem *stem = state.segment.stem;
	const Path path = stem->getPath();
	const vector<Joint> joints = stem->getJoints();
	incrementJoint(state, joints);
	size_t pathIndex = joints[state.jointIndex].getPathIndex();

	if (state.jointIndex == 0 && state.section <= pathIndex) {
		weights.x = 1.0f;
		weights.y = 0.0f;
		indices.x = static_cast<float>(state.jointID);
		indices.y = indices.x;
	} else if (state.section == 0 || state.section == path.getSize()-1) {
		weights.x = 1.0f;
		weights.y = 0.0f;
		indices.x = static_cast<float>(state.jointID);
		indices.y = indices.x;
	} else if (state.section == pathIndex) {
		unsigned prevID = joints[state.jointIndex-1].getID();
		weights.x = 0.5f;
		weights.y = 0.5f;
		indices.x = static_cast<float>(state.jointID);
		indices.y = static_cast<float>(prevID);
	} else {
		if (state.section != pathIndex) {
			Vec3 point1 = stem->getPath().get(state.section);
			Vec3 point2 = stem->getPath().get(state.section - 1);
			state.jointOffset += magnitude(point1-point2);
		}
		setJointInfo(stem, state.jointOffset, state.jointIndex,
			weights, indices);
	}
}

void Mesh::setJointInfo(const Stem *stem, float jointOffset, size_t jointIndex,
	Vec2 &weights, Vec2 &indices)
{
	const vector<Joint> joints = stem->getJoints();
	const Path path = stem->getPath();
	size_t pathIndex = joints[jointIndex].getPathIndex();
	unsigned jointID = joints[jointIndex].getID();

	float ratio;
	float distance;
	bool lastJoint = jointIndex + 1 >= joints.size();
	if (lastJoint) {
		size_t start = pathIndex;
		size_t end = path.getSize() - 1;
		distance = path.getDistance(start, end);
		ratio = jointOffset / distance;
	} else {
		Joint nextJoint = joints[jointIndex + 1];
		size_t start = pathIndex;
		size_t end = nextJoint.getPathIndex();
		distance = path.getDistance(start, end);
		ratio = jointOffset / distance;
	}

	bool first = ratio < 0.5f && jointIndex == 0;
	bool last = ratio > 0.5f && lastJoint;
	if (ratio == 0.5f || first || last) {
		weights.x = 1.0f;
		weights.y = 0.0f;
		indices.x = static_cast<float>(jointID);
		indices.y = indices.x;
	} else if (ratio > 0.5f) {
		int nextID = joints[jointIndex + 1].getID();
		indices.x = static_cast<float>(jointID);
		indices.y = static_cast<float>(nextID);
		float ratio = jointOffset / distance - 0.5f;
		weights.x = 1.0f - ratio;
		weights.y = ratio;
	} else {
		int prevID = joints[jointIndex - 1].getID();
		indices.x = static_cast<float>(jointID);
		indices.y = static_cast<float>(prevID);
		float ratio = jointOffset / distance;
		weights.x = 0.5f + ratio;
		weights.y = 0.5f - ratio;
	}
}

void Mesh::addTriangle(int mesh, int a, int b, int c)
{
	this->indices[mesh].push_back(a);
	this->indices[mesh].push_back(b);
	this->indices[mesh].push_back(c);
}

/** Different buffers are used for different materials. This is done to keep
geometry with identical materials together and simplify draw calls. */
int Mesh::selectBuffer(long material)
{
	return material;
}

void Mesh::initBuffer()
{
	size_t size = this->plant->getMaterials().size();
	this->vertices.resize(size);
	this->indices.resize(size);
	this->stemSegments.resize(size);
	this->leafSegments.resize(size);
	for (size_t i = 0; i < size; i++) {
		this->vertices[i].clear();
		this->indices[i].clear();
		this->stemSegments[i].clear();
		this->leafSegments[i].clear();
	}
}

/** Geometry is divided into different groups depending on material.
Geometry is latter stored in the same vertex buffer but is separated based
on material to minimize draw calls. This method updates the indices to what
they should be in the final merged vertex buffer. */
void Mesh::updateSegments()
{
	if (!this->indices.empty()) {
		unsigned vsize = this->vertices[0].size();
		unsigned isize = this->indices[0].size();
		for (unsigned mesh = 1; mesh < this->indices.size(); mesh++) {
			for (unsigned &index : this->indices[mesh])
				index += vsize;
			for (auto &pair : this->stemSegments[mesh]) {
				Segment *segment = &pair.second;
				segment->vertexStart += vsize;
				segment->indexStart += isize;
			}
			for (auto &pair : this->leafSegments[mesh]) {
				Segment *segment = &pair.second;
				segment->vertexStart += vsize;
				segment->indexStart += isize;
			}
			vsize += this->vertices[mesh].size();
			isize += this->indices[mesh].size();
		}
	}
}

size_t Mesh::getMeshCount() const
{
	return this->indices.size();
}

size_t Mesh::getVertexCount() const
{
	size_t size = 0;
	for (auto &mesh : this->vertices)
		size += mesh.size();
	return size;
}

size_t Mesh::getIndexCount() const
{
	size_t size = 0;
	for (auto &mesh : this->indices)
		size += mesh.size();
	return size;
}

unsigned Mesh::getMaterialIndex(int mesh) const
{
	return mesh;
}

vector<DVertex> Mesh::getVertices() const
{
	vector<DVertex> object;
	for (auto it = this->vertices.begin(); it != this->vertices.end(); it++)
		object.insert(object.end(), it->begin(), it->end());
	return object;
}

vector<unsigned> Mesh::getIndices() const
{
	vector<unsigned> object;
	for (auto it = this->indices.begin(); it != this->indices.end(); it++)
		object.insert(object.end(), it->begin(), it->end());
	return object;
}

const vector<DVertex> *Mesh::getVertices(int mesh) const
{
	return &this->vertices.at(mesh);
}

const vector<unsigned> *Mesh::getIndices(int mesh) const
{
	return &this->indices.at(mesh);
}

map<Mesh::LeafID, Segment> Mesh::getLeaves(int mesh) const
{
	return this->leafSegments.at(mesh);
}

size_t Mesh::getLeafCount(int mesh) const
{
	return this->leafSegments.at(mesh).size();
}

Segment Mesh::findStem(Stem *stem) const
{
	Segment segment = {};
	for (size_t i = 0; i < this->stemSegments.size(); i++) {
		try {
			segment = this->stemSegments[i].at(stem);
			break;
		} catch (std::out_of_range) {}
	}
	return segment;
}

Segment Mesh::findLeaf(LeafID leaf) const
{
	Segment segment = {};
	for (size_t i = 0; i < this->leafSegments.size(); i++) {
		try {
			segment = this->leafSegments[i].at(leaf);
			break;
		} catch (std::out_of_range) {}
	}
	return segment;
}
