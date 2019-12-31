/* Plant Genererator
 * Copyright (C) 2019  Floris Creyf
 *
 * Plant Genererator is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Plant Genererator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "add_leaf.h"

AddLeaf::AddLeaf(Selection *selection, const Camera *camera, int x, int y) :
	prevSelection(*selection), moveStem(selection, camera, x, y, true)
{
	this->selection = selection;
	this->stem = nullptr;
}

void AddLeaf::execute()
{
	auto instances = this->selection->getStemInstances();
	if (instances.size() == 1) {
		this->leaf = pg::Leaf();
		this->stem = (*instances.begin()).first;
		this->stem->addLeaf(this->leaf);
		this->selection->clear();
		this->selection->addLeaf(this->stem, this->leaf.getID());
		this->moveStem.execute();
	}
}

bool AddLeaf::onMouseMove(QMouseEvent *event)
{
	return this->moveStem.onMouseMove(event);
}

bool AddLeaf::onMousePress(QMouseEvent *event)
{
	bool update = this->moveStem.onMousePress(event);
	this->done = this->moveStem.isDone();
	return update;
}

void AddLeaf::undo()
{
	this->leaf = *this->stem->getLeaf(this->leaf.getID());
	this->stem->removeLeaf(this->leaf.getID());
	*this->selection = this->prevSelection;
}

void AddLeaf::redo()
{
	auto instances = this->selection->getStemInstances();
	if (instances.size( ) == 1) {
		pg::Stem *stem = (*instances.begin()).first;
		this->stem->addLeaf(this->leaf);
		this->selection->clear();
		this->selection->addLeaf(stem, this->leaf.getID());
	}
}