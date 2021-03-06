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

#ifndef PG_CURVE_H
#define PG_CURVE_H

#include "spline.h"
#include <string>

#ifdef PG_SERIALIZE
#include <boost/archive/text_oarchive.hpp>
#endif

namespace pg {
	class Curve {
		Spline spline;
		std::string name;

#ifdef PG_SERIALIZE
		friend class boost::serialization::access;
		template<class Archive>
		void serialize(Archive &ar, const unsigned)
		{
			ar & spline;
			ar & name;
		}
#endif

	public:
		Curve();
		Curve(int type);
		Curve(Spline spline);
		Curve(Spline spline, std::string name);
		void setName(std::string name);
		std::string getName() const;
		void setSpline(Spline spline);
		Spline getSpline() const;
	};
}

#endif
