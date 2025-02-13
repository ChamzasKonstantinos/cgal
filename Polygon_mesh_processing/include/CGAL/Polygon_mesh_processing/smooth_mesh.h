// Copyright (c) 2018 GeometryFactory (France).
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org).
// You can redistribute it and/or modify it under the terms of the GNU
// General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
//
// Licensees holding a valid commercial license may use this file in
// accordance with the commercial license agreement provided with the software.
//
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
//
// $URL$
// $Id$
// SPDX-License-Identifier: GPL-3.0+
//
//
// Author(s)     : Mael Rouxel-Labbé
//                 Konstantinos Katrioplas (konst.katrioplas@gmail.com)

#ifndef CGAL_POLYGON_MESH_PROCESSING_SMOOTH_MESH_H
#define CGAL_POLYGON_MESH_PROCESSING_SMOOTH_MESH_H

#include <CGAL/license/Polygon_mesh_processing/meshing_hole_filling.h>

#include <CGAL/Polygon_mesh_processing/internal/Smoothing/mesh_smoothing_impl.h>
#include <CGAL/Polygon_mesh_processing/internal/Smoothing/smoothing_evaluation.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>

#include <CGAL/boost/graph/Named_function_parameters.h>
#include <CGAL/boost/graph/named_params_helper.h>

#include <CGAL/property_map.h>

#ifdef DOXYGEN_RUNNING
#define CGAL_PMP_NP_TEMPLATE_PARAMETERS NamedParameters
#define CGAL_PMP_NP_CLASS NamedParameters
#endif

namespace CGAL {
namespace Polygon_mesh_processing {

/*!
* \ingroup PMP_meshing_grp
*
* \short smoothes a triangulated region of a polygon mesh.
*
* This function attempts to make the triangle angle and area distributions as uniform as possible
* by moving (non-constrained) vertices.
*
* Angle-based smoothing does not change the combinatorial information of the mesh. Area-based smoothing
* might change the combinatorial information, unless specified otherwise. It is also possible
* to make the smoothing algorithm "safer" by rejecting moves that, when applied, would worsen the
* quality of the mesh, e.g. that would decrease the value of the smallest angle around a vertex or
* create self-intersections.
*
* Optionally, the points are reprojected after each iteration.
*
* @tparam TriangleMesh model of `MutableFaceGraph`.
* @tparam FaceRange range of `boost::graph_traits<TriangleMesh>::%face_descriptor`,
          model of `Range`. Its iterator type is `ForwardIterator`.
* @tparam NamedParameters a sequence of \ref pmp_namedparameters "Named Parameters"
*
* @param tmesh a polygon mesh with triangulated surface patches to be smoothed.
* @param faces the range of triangular faces defining one or several surface patches to be smoothed.
* @param np optional sequence of \ref pmp_namedparameters "Named Parameters" among the ones listed below.
*
* \cgalNamedParamsBegin
*  \cgalParamBegin{use_angle_smoothing} Boolean value to indicate whether angle-based smoothing should be used.
*    %Default is `true`.
*  \cgalParamEnd
*  \cgalParamBegin{use_area_smoothing} Boolean value to indicate whether area-based smoothing should be used.
*    %Default is `true`.
*  \cgalParamEnd
*  \cgalParamBegin{number_of_iterations} the number of iterations for the
*    sequence of the smoothing iterations performed (default is 1).
*  \cgalParamEnd
*  \cgalParamBegin{use_safety_constraints} if `true`, vertex moves that would worsen the mesh
*    are ignored. %Default is `false`.
*  \cgalParamEnd
*  \cgalParamBegin{use_Delaunay_flips} if `true` (default value), area-based smoothing will be completed
*    by a phase of Delaunay-based edge-flips to prevent the creation of elongated triangles.
*  \cgalParamEnd
*  \cgalParamBegin{do_project} if `true` (default value), points are projected onto the initial surface
*   after each iteration.
*  \cgalParamEnd
*  \cgalParamBegin{vertex_is_constrained_map} a property map containing the
*    constrained-or-not status of each vertex of `tmesh`. A constrained vertex
*    cannot be modified at all during smoothing.
*  \cgalParamEnd
*  \cgalParamBegin{edge_is_constrained_map} a property map, model of `ReadWritePropertyMap`, containing the
*    constrained-or-not status of each edge of `tmesh`. A constrained edge cannot be flipped and its extremities
*    are tagged as constrained vertices.
*  \cgalParamEnd
*  \cgalParamBegin{vertex_point_map} the property map, model of `ReadWritePropertyMap`, with the points
*    associated to the vertices of `tmesh`.
*  \cgalParamEnd
*  \cgalParamBegin{geom_traits} a geometric traits class instance, model of `Kernel`.
*    Exact constructions kernels are not supported by this function.
*  \cgalParamEnd
* \cgalNamedParamsEnd
*
* @warning The third party library \link thirdpartyCeres Ceres \endlink is required
* to use area-based smoothing.
*
* @pre `tmesh` does not contain any degenerate faces
*/
template<typename TriangleMesh, typename FaceRange, typename NamedParameters>
void smooth_mesh(const FaceRange& faces,
                 TriangleMesh& tmesh,
                 const NamedParameters& np)
{
  typedef typename boost::graph_traits<TriangleMesh>::vertex_descriptor               vertex_descriptor;
  typedef typename boost::graph_traits<TriangleMesh>::halfedge_descriptor             halfedge_descriptor;
  typedef typename boost::graph_traits<TriangleMesh>::edge_descriptor                 edge_descriptor;
  typedef typename boost::graph_traits<TriangleMesh>::face_descriptor                 face_descriptor;

  typedef typename GetGeomTraits<TriangleMesh, NamedParameters>::type                 GeomTraits;
  typedef typename GetVertexPointMap<TriangleMesh, NamedParameters>::type             VertexPointMap;

  // We need a default pmap that is not just 'constant_pmap(false)' because if an edge is constrained,
  // its vertices are constrained.
  typedef CGAL::dynamic_vertex_property_t<bool>                                       Vertex_property_tag;
  typedef typename boost::property_map<TriangleMesh, Vertex_property_tag>::type       Default_VCMap;
  typedef typename internal_np::Lookup_named_param_def<internal_np::vertex_is_constrained_t,
                                                 NamedParameters,
                                                 Default_VCMap
                                                 > ::type                             VCMap;

  typedef typename internal_np::Lookup_named_param_def<internal_np::edge_is_constrained_t,
                                                 NamedParameters,
                                                 Constant_property_map<edge_descriptor, bool> // default
                                                 > ::type                             ECMap;

  typedef internal::Area_smoother<TriangleMesh, VertexPointMap, GeomTraits>           Area_optimizer;
  typedef internal::Mesh_smoother<Area_optimizer, TriangleMesh,
                                  VertexPointMap, VCMap, GeomTraits>                  Area_smoother;
  typedef internal::Delaunay_edge_flipper<TriangleMesh, VertexPointMap,
                                          ECMap, GeomTraits>                          Delaunay_flipper;

  typedef internal::Angle_smoother<TriangleMesh, VertexPointMap, GeomTraits>          Angle_optimizer;
  typedef internal::Mesh_smoother<Angle_optimizer, TriangleMesh,
                                  VertexPointMap, VCMap, GeomTraits>                  Angle_smoother;

  typedef typename GeomTraits::Triangle_3                                             Triangle;
  typedef std::vector<Triangle>                                                       Triangle_container;

  typedef CGAL::AABB_triangle_primitive<GeomTraits,
                                        typename Triangle_container::iterator>        AABB_Primitive;
  typedef CGAL::AABB_traits<GeomTraits, AABB_Primitive>                               AABB_Traits;
  typedef CGAL::AABB_tree<AABB_Traits>                                                Tree;

  if(std::begin(faces) == std::end(faces))
    return;

  using parameters::choose_parameter;
  using parameters::get_parameter;

  // named parameters
  GeomTraits gt = choose_parameter(get_parameter(np, internal_np::geom_traits),
                               GeomTraits());
  VertexPointMap vpmap = choose_parameter(get_parameter(np, internal_np::vertex_point),
                               get_property_map(CGAL::vertex_point, tmesh));

  const bool use_angle_smoothing = choose_parameter(get_parameter(np, internal_np::use_angle_smoothing), true);
  bool use_area_smoothing = choose_parameter(get_parameter(np, internal_np::use_area_smoothing), true);

#ifndef CGAL_PMP_USE_CERES_SOLVER
  std::cerr << "Area-based smoothing requires the Ceres Library, which is not available." << std::endl;
  std::cerr << "No such smoothing will be performed!" << std::endl;
  use_area_smoothing = false;
#endif

  if(!use_angle_smoothing && !use_area_smoothing)
    std::cerr << "Called PMP::smooth_mesh() without any smoothing method selected or available" << std::endl;

  unsigned int nb_iterations = choose_parameter(get_parameter(np, internal_np::number_of_iterations), 1);
  const bool do_project = choose_parameter(get_parameter(np, internal_np::do_project), true);
  const bool use_safety_constraints = choose_parameter(get_parameter(np, internal_np::use_safety_constraints), true);
  const bool use_Delaunay_flips = choose_parameter(get_parameter(np, internal_np::use_Delaunay_flips), true);

  VCMap vcmap = choose_parameter(get_parameter(np, internal_np::vertex_is_constrained),
                             get(Vertex_property_tag(), tmesh));

  // If it's the default vcmap, manually set everything to false because the dynamic pmap has no default initialization
  if((std::is_same<VCMap, Default_VCMap>::value))
  {
    for(vertex_descriptor v : vertices(tmesh))
      put(vcmap, v, false);
  }

  ECMap ecmap = choose_parameter(get_parameter(np, internal_np::edge_is_constrained),
                             Constant_property_map<edge_descriptor, bool>(false));

  // a constrained edge has constrained extremities
  for(face_descriptor f : faces)
  {
    if(f == boost::graph_traits<TriangleMesh>::null_face())
      continue;

    for(halfedge_descriptor h : CGAL::halfedges_around_face(halfedge(f, tmesh), tmesh))
    {
      if(get(ecmap, edge(h, tmesh)))
      {
        put(vcmap, source(h, tmesh), true);
        put(vcmap, target(h, tmesh), true);
      }
    }
  }

  // Construct the AABB tree (if needed for reprojection)
  std::vector<Triangle> input_triangles;

  if(do_project)
  {
    input_triangles.reserve(faces.size());

    for(face_descriptor f : faces)
    {
      halfedge_descriptor h = halfedge(f, tmesh);
      if(is_border(h, tmesh)) // should not happen, but just in case
        continue;

      input_triangles.push_back(gt.construct_triangle_3_object()(get(vpmap, source(h, tmesh)),
                                                                 get(vpmap, target(h, tmesh)),
                                                                 get(vpmap, target(next(h, tmesh), tmesh))));
    }
  }

  Tree aabb_tree(input_triangles.begin(), input_triangles.end());
  aabb_tree.accelerate_distance_queries();

  // Setup the working ranges and check some preconditions
  Angle_smoother angle_smoother(tmesh, vpmap, vcmap, gt);
  Area_smoother area_smoother(tmesh, vpmap, vcmap, gt);
  Delaunay_flipper delaunay_flipper(tmesh, vpmap, ecmap, gt);

  if(use_angle_smoothing)
    angle_smoother.init_smoothing(faces);

  if(use_area_smoothing)
    area_smoother.init_smoothing(faces);

  for(unsigned int i=0; i<nb_iterations; ++i)
  {
    if(use_area_smoothing)
    {
      // First apply area smoothing...
      area_smoother.optimize(use_safety_constraints /*check for bad faces*/,
                             false /*apply moves as soon as they're calculated*/,
                             false /*do not enforce a minimum angle improvement*/);
      if(do_project)
      {
        if(use_safety_constraints && does_self_intersect(tmesh))
        {
#ifdef CGAL_PMP_SMOOTHING_VERBOSE
          std::cerr << "Cannot re-project as there are self-intersections in the mesh!\n";
#endif
          break;
        }

        area_smoother.project_to_surface(aabb_tree);
      }

      if(use_Delaunay_flips)
        delaunay_flipper(faces);
    }

    // ... then angle smoothing
    if(use_angle_smoothing)
    {
      angle_smoother.optimize(use_safety_constraints /*check for bad faces*/,
                              true /*apply all moves at once*/,
                              use_safety_constraints /*check if the min angle is improved*/);

      if(do_project)
      {
        if(use_safety_constraints && does_self_intersect(tmesh))
        {
#ifdef CGAL_PMP_SMOOTHING_VERBOSE
          std::cerr << "Can't do re-projection, there are self-intersections in the mesh!\n";
#endif
          break;
        }

        angle_smoother.project_to_surface(aabb_tree);
      }
    }
  }
}

template <typename FaceRange, typename TriangleMesh>
void smooth_mesh(const FaceRange& face_range, TriangleMesh& tmesh)
{
  smooth_mesh(face_range, tmesh, parameters::all_default());
}

template <typename TriangleMesh, typename CGAL_PMP_NP_TEMPLATE_PARAMETERS>
void smooth_mesh(TriangleMesh& tmesh, const CGAL_PMP_NP_CLASS& np)
{
  smooth_mesh(faces(tmesh), tmesh, np);
}

template<typename TriangleMesh>
void smooth_mesh(TriangleMesh& tmesh)
{
  smooth_mesh(faces(tmesh), tmesh, parameters::all_default());
}


///\cond SKIP_IN_MANUAL
template<typename TriangleMesh, typename GeomTraits, typename Stream>
void angles_evaluation(TriangleMesh& tmesh, GeomTraits traits, Stream& output)
{
  internal::Quality_evaluator<TriangleMesh, GeomTraits> evaluator(tmesh, traits);
  evaluator.gather_angles();
  evaluator.extract_angles(output);
}

template<typename TriangleMesh, typename GeomTraits, typename Stream>
void areas_evaluation(TriangleMesh& tmesh, GeomTraits traits, Stream& output)
{
  internal::Quality_evaluator<TriangleMesh, GeomTraits> evaluator(tmesh, traits);
  evaluator.measure_areas();
  evaluator.extract_areas(output);
}

template<typename TriangleMesh, typename GeomTraits, typename Stream>
void aspect_ratio_evaluation(TriangleMesh& tmesh, GeomTraits traits, Stream& output)
{
  internal::Quality_evaluator<TriangleMesh, GeomTraits> evaluator(tmesh, traits);
  evaluator.calc_aspect_ratios();
  evaluator.extract_aspect_ratios(output);
}
///\endcond SKIP_IN_MANUAL

} // namespace Polygon_mesh_processing
} // namespace CGAL

#endif // CGAL_POLYGON_MESH_PROCESSING_SMOOTH_MESH_H
