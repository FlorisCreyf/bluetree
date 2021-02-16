/* Copyright 2020 Floris Creyf
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

#ifndef PG_GENERATOR_H
#define PG_GENERATOR_H

#include "plant.h"
#include "mesh.h"
#include "volume.h"
#include "math/intersection.h"
#include <vector>
#include <map>

namespace pg {
	class Generator {
		Plant *plant;
		float primaryGrowthRate;
		float secondaryGrowthRate;
		float minRadius;
		int rayCount;
		int rayLevels;
		float width;

		void initRoot();
		void addToVolume(Volume *, Stem *);
		void castRays(Volume *);
		void updateRadiantEnergy(Volume *, Ray);
		void generalizeDensity(Volume::Node *);
		void generalizeFlux(Volume::Node *);
		float evaluateEfficiency(Volume *, Stem *);
		void addNodes(Volume *, Stem *, int);
		void addNode(Volume *, Stem *, int);
		void updateRadius(Stem *);
		void addStems(Stem *, Volume *);
		void addStem(Stem *, Volume *);
		void addLeaves(Stem *, int);
		Leaf createLeaf();
		void updateBoundingBox(Vec3);

	public:
		Generator(Plant *plant);
		void grow(int cycles, int nodes);
		void setPrimaryGrowthRate(float rate);
		void setSecondaryGrowthRate(float rate);
		void setRayDensity(int baseCount, int levels);
	};
}

#endif
