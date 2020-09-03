#include "path_planner.h"
#include "path_point.h"
#include <iostream>
#include <algorithm>
#include <cmath> 

PathPlanner::PathPlanner(float car_x, float car_y, std::vector<Cone> cones, bool const_velocity, uint8_t v_max, uint8_t v_const, uint8_t max_f_gain)
    : const_velocity(const_velocity), v_max(v_max), v_const(v_const), max_f_gain(max_f_gain) 
{
    int num_l = 0;
    int num_r = 0;

    // Add to list of raw_cones so that references can be made to be amended
    for (auto& cone: cones) {raw_cones.push_back(cone);}

    // Prepare initial cones to be added to the path planner 
	for (auto& cone: cones) 
    {
		if (cone.colour == 'b')
		{
			num_l++;
			l_cones_to_add.push_back(&cone);
			l_cones_sorted = false;
		}
		else if (cone.colour == 'y')
		{
			num_r++;
			r_cones_to_add.push_back(&cone);
			r_cones_sorted = false;
		}
		else if (cone.colour == 'r')
		{
		   timing_cones.push_back(&cone); 
		}
    }
    
    // Add the car position to the centre points (THIS HAS BEEN TRUNCATED - confirm with Alex)
    PathPoint car_pos = PathPoint(car_x, car_y);
    centre_points.push_back(car_pos);

    // Sort by distance to car 
    sortConesByDist(car_pos, car_pos);
	
    // Add CLOSEST cones to vector of known cones
    left_cones.push_back(*l_cones_to_add.begin());
    right_cones.push_back(*r_cones_to_add.begin());

    // Clear pointers and reset l/r_cones_to_add
    l_cones_to_add.erase(l_cones_to_add.begin());
    r_cones_to_add.erase(r_cones_to_add.begin());
    l_cones_sorted = false;
    r_cones_sorted = false;

    // Re-sort by distance to closest cone
    sortConesByDist((*left_cones[0]).position, (*right_cones[0]).position);
    popConesToAdd();
    addFirstCentrePoints();
}

void PathPlanner::addFirstCentrePoints()
{
    size_t n = std::min(left_cones.size(), right_cones.size());		
    size_t closest_opp_idx;
    float centre_x, centre_y;

    for (int i = 0; i < n; i++)
    {
	if (~left_cones[i]->mapped)
	{
	    closest_opp_idx = findOppositeClosest(*left_cones[i], right_cones);
	    if (closest_opp_idx != -1)
	    {
		centre_x = (right_cones[closest_opp_idx]->position.x + left_cones[i]->position.x) / 2;
		centre_y = (right_cones[closest_opp_idx]->position.y + left_cones[i]->position.y) / 2;
		centre_points.push_back(PathPoint(centre_x, centre_y));
		left_cones[i]->mapped = true;
		right_cones[closest_opp_idx]->mapped = true;
		l_cone_index++;
	    }
	}
    }
}

int PathPlanner::findOppositeClosest(const Cone &cone, const std::vector<Cone*> &cones)
{
	float min_dist = 6;
	float dist;
	int index = -1;
	int i = 0;

	for (auto it = cones.rbegin(); it != cones.rend(); it++)
	{
	    dist = std::hypot(cone.position.x - (**it).position.x, cone.position.y - (**it).position.y);

	    if (dist < min_dist)
	    {
		min_dist = dist;
		index = i;
	    }
	    i++;
	}
	return index;
}

void PathPlanner::popConesToAdd()
{
	bool need_add_more = true;	
	bool was_added = true;	
	float dist;

	while (need_add_more && was_added)
	{
		was_added = false;
		size_t num_left = l_cones_to_add.size();
		size_t num_right = r_cones_to_add.size();

		if (num_left > 0)	
		{
			dist = std::hypot(l_cones_to_add[0]->position.x, l_cones_to_add[0]->position.y);
			if (dist < 5)
			{
				left_cones.push_back(l_cones_to_add[0]);
				l_cones_to_add.erase(l_cones_to_add.begin());
				l_cones_sorted = false;
				was_added = true;
			}
		}
		if (num_right > 0)
		{
			dist = std::hypot(r_cones_to_add[0]->position.x, r_cones_to_add[0]->position.y);
			if (dist < 5)
			{
				right_cones.push_back(r_cones_to_add[0]);
				r_cones_to_add.erase(r_cones_to_add.begin());
				r_cones_sorted = false;
				was_added = true;
			}
		}
		if (l_cones_to_add.size() + r_cones_to_add.size() > 0)
		{
			need_add_more = true;
			sortConesByDist((*l_cones_to_add.end())->position, (*r_cones_to_add.end())->position);
		}
		else {break;}
	}
}

void PathPlanner::sortConesByDist(PathPoint &left, PathPoint &right)
{
	// Assign distance Cone objects on left
    for (auto &cone: l_cones_to_add)
    {cone->dist = std::hypot(left.x - cone->position.x, left.y - cone->position.y);}

	// Assign distance to Cone objects on right
    for (auto &cone: r_cones_to_add) 
    {cone->dist = std::hypot(right.x - cone->position.x, right.y - cone->position.y);}

	// nlogn sort both cones_to_add vectors
    sort(l_cones_to_add.begin(), l_cones_to_add.end(), compareConeDist);
    sort(r_cones_to_add.begin(), r_cones_to_add.end(), compareConeDist);

	l_cones_sorted = true;
	r_cones_sorted = true;
}

void PathPlanner::resetTempConeVectors()
{
	for (auto &element: this->l_cones_to_add) {delete element;};
	for (auto &element: this->r_cones_to_add) {delete element;};
	this->l_cones_to_add.clear();
	this->r_cones_to_add.clear();
	this->l_cones_sorted = false;
	this->r_cones_sorted = false;
}

bool PathPlanner::compareConeDist(Cone* const &cone_one, Cone* const &cone_two)
{
    return cone_one->dist < cone_two->dist;
}

     
