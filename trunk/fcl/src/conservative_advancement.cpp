/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/** \author Jia Pan */

#include "fcl/conservative_advancement.h"

#include "fcl/collision_node.h"
#include "fcl/traversal_node_bvhs.h"
#include "fcl/simple_setup.h"
#include "fcl/motion.h"
#include "fcl/collision.h"
#include "fcl/traversal_recurse.h"

namespace fcl
{

int conservativeAdvancement(const CollisionObject* o1,
                            const Vec3f R1_1[3], const Vec3f& T1_1,
                            const Vec3f R1_2[3], const Vec3f& T1_2,
                            const CollisionObject* o2,
                            const Vec3f R2_1[3], const Vec3f& T2_1,
                            const Vec3f R2_2[3], const Vec3f& T2_2,
                            int num_max_contacts, bool exhaustive, bool enable_contact,
                            std::vector<Contact>& contacts,
                            BVH_REAL& toc)
{
  if(num_max_contacts <= 0 && !exhaustive)
  {
    std::cerr << "Warning: should stop early as num_max_contact is " << num_max_contacts << " !" << std::endl;
    return 0;
  }

  const OBJECT_TYPE object_type1 = o1->getObjectType();
  const NODE_TYPE node_type1 = o1->getNodeType();

  const OBJECT_TYPE object_type2 = o2->getObjectType();
  const NODE_TYPE node_type2 = o2->getNodeType();

  if(object_type1 != OT_BVH || object_type2 != OT_BVH)
    return 0;

  if(node_type1 != BV_RSS || node_type2 != BV_RSS)
    return 0;

  MeshConservativeAdvancementTraversalNodeRSS node;

  const BVHModel<RSS>* model1 = (const BVHModel<RSS>*)o1;
  const BVHModel<RSS>* model2 = (const BVHModel<RSS>*)o2;

  initialize(node, *model1, *model2, R1_1, T1_1, R2_1, T2_1);

  node.motion1 = new InterpMotion<RSS>(R1_1, T1_1, R1_2, T1_2);
  node.motion2 = new InterpMotion<RSS>(R2_1, T2_1, R2_2, T2_2);

  int b = collide(o1, o2, num_max_contacts, exhaustive, enable_contact, contacts);

  if(b > 0)
  {
    toc = 0;
    return b;
  }

  do
  {
    Vec3f R1_t[3];
    Vec3f R2_t[3];
    Vec3f T1_t;
    Vec3f T2_t;

    node.motion1->getCurrentTransform(R1_t, T1_t);
    node.motion2->getCurrentTransform(R2_t, T2_t);

    // compute the transformation from 1 to 2
    relativeTransform(R1_t, T1_t, R2_t, T2_t, node.R, node.T);

    node.delta_t = 1;
    node.min_distance = std::numeric_limits<BVH_REAL>::max();

    distanceRecurse(&node, 0, 0, NULL);

    if(node.delta_t < node.t_err)
      break;

    node.toc += node.delta_t;

    if(node.toc > 1)
    {
      node.toc = 1;
      break;
    }

    node.motion1->integrate(node.toc);
    node.motion2->integrate(node.toc);
  }
  while(1);

  if(node.toc < 1)
    return 1;

  return 0;
}

}
