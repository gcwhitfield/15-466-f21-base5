#include "WalkMesh.hpp"

#include "read_write_chunk.hpp"

#include <glm/gtx/norm.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>

WalkMesh::WalkMesh(std::vector< glm::vec3 > const &vertices_, std::vector< glm::vec3 > const &normals_, std::vector< glm::uvec3 > const &triangles_)
	: vertices(vertices_), normals(normals_), triangles(triangles_) {

	//construct next_vertex map (maps each edge to the next vertex in the triangle):
	next_vertex.reserve(triangles.size()*3);
	auto do_next = [this](uint32_t a, uint32_t b, uint32_t c) {
		auto ret = next_vertex.insert(std::make_pair(glm::uvec2(a,b), c));
		assert(ret.second);
	};
	for (auto const &tri : triangles) {
		do_next(tri.x, tri.y, tri.z);
		do_next(tri.y, tri.z, tri.x);
		do_next(tri.z, tri.x, tri.y);
	}

	//DEBUG: are vertex normals consistent with geometric normals?
	for (auto const &tri : triangles) {
		glm::vec3 const &a = vertices[tri.x];
		glm::vec3 const &b = vertices[tri.y];
		glm::vec3 const &c = vertices[tri.z];
		glm::vec3 out = glm::normalize(glm::cross(b-a, c-a));

		float da = glm::dot(out, normals[tri.x]);
		float db = glm::dot(out, normals[tri.y]);
		float dc = glm::dot(out, normals[tri.z]);

		assert(da > 0.1f && db > 0.1f && dc > 0.1f);
	}
}

//project pt to the plane of triangle a,b,c and return the barycentric weights of the projected point:
glm::vec3 barycentric_weights(glm::vec3 const &a, glm::vec3 const &b, glm::vec3 const &c, glm::vec3 const &pt) {
	// use Professor McCann's solution from class
    glm::vec3 h = glm::cross(c - a, b - c);
    // compute projected, signed areas of smaller triangles formed with the point 
	// input arguments to the cross functions taken frmo discord
    float A = glm::dot(glm::cross(b - pt, c - pt), h);
    float B = glm::dot(glm::cross(c - pt, a - pt), h);
    float C = glm::dot(glm::cross(a - pt, b - pt), h);
    float S = A + B + C;
	return glm::vec3(A, B, C) / S;
}

WalkPoint WalkMesh::nearest_walk_point(glm::vec3 const &world_point) const {
	assert(!triangles.empty() && "Cannot start on an empty walkmesh");

	WalkPoint closest;
	float closest_dis2 = std::numeric_limits< float >::infinity();

	for (auto const &tri : triangles) {
		//find closest point on triangle:

		glm::vec3 const &a = vertices[tri.x];
		glm::vec3 const &b = vertices[tri.y];
		glm::vec3 const &c = vertices[tri.z];

		//get barycentric coordinates of closest point in the plane of (a,b,c):
		glm::vec3 coords = barycentric_weights(a,b,c, world_point);

		//is that point inside the triangle?
		if (coords.x >= 0.0f && coords.y >= 0.0f && coords.z >= 0.0f) {
			//yes, point is inside triangle.
			float dis2 = glm::length2(world_point - to_world_point(WalkPoint(tri, coords)));
			if (dis2 < closest_dis2) {
				closest_dis2 = dis2;
				closest.indices = tri;
				closest.weights = coords;
			}
		} else {
			//check triangle vertices and edges:
			auto check_edge = [&world_point, &closest, &closest_dis2, this](uint32_t ai, uint32_t bi, uint32_t ci) {
				glm::vec3 const &a = vertices[ai];
				glm::vec3 const &b = vertices[bi];

				//find closest point on line segment ab:
				float along = glm::dot(world_point-a, b-a);
				float max = glm::dot(b-a, b-a);
				glm::vec3 pt;
				glm::vec3 coords;
				if (along < 0.0f) {
					pt = a;
					coords = glm::vec3(1.0f, 0.0f, 0.0f);
				} else if (along > max) {
					pt = b;
					coords = glm::vec3(0.0f, 1.0f, 0.0f);
				} else {
					float amt = along / max;
					pt = glm::mix(a, b, amt);
					coords = glm::vec3(1.0f - amt, amt, 0.0f);
				}

				float dis2 = glm::length2(world_point - pt);
				if (dis2 < closest_dis2) {
					closest_dis2 = dis2;
					closest.indices = glm::uvec3(ai, bi, ci);
					closest.weights = coords;
				}
			};
			check_edge(tri.x, tri.y, tri.z);
			check_edge(tri.y, tri.z, tri.x);
			check_edge(tri.z, tri.x, tri.y);
		}
	}
	assert(closest.indices.x < vertices.size());
	assert(closest.indices.y < vertices.size());
	assert(closest.indices.z < vertices.size());
	return closest;
}


void WalkMesh::walk_in_triangle(WalkPoint const &start, glm::vec3 const &step, WalkPoint *end_, float *time_) const {
	
	assert(end_);
	auto &end = *end_;

	assert(time_);
	auto &time = *time_;

	assert(start.weights.x == start.weights.x);
	assert(start.weights.y == start.weights.y);
	assert(start.weights.z == start.weights.z);
	assert(step.x == step.x);
	assert(step.y == step.y);
	assert(step.z == step.z);

	assert (start.weights.x <= 1);
	assert (start.weights.y <= 1);
	assert (start.weights.z <= 1);
	assert (start.weights.x >= 0);
	assert (start.weights.y >= 0);
	assert (start.weights.z >= 0);

    glm::vec3 const &a = vertices[start.indices.x];
	glm::vec3 const &b = vertices[start.indices.y];
	glm::vec3 const &c = vertices[start.indices.z];
	
	glm::vec3 step_coords =  barycentric_weights(a, b, c, step) - barycentric_weights(a, b, c, glm::vec3(0.0f, 0.0f, 0.0f));

	//if no edge is crossed, event will just be taking the whole step:
	time = 1.0f;
	end = start;

	//figure out which edge (if any) is crossed first.
	// set time and end appropriately.
	// glm::vec3 end_coords = start.weights + step_coords;
	glm::vec3 t = glm::vec3(
		-start.weights.x / step_coords.x,
		-start.weights.y / step_coords.y,
		-start.weights.z / step_coords.z
	);

	auto min = [] (glm::vec3 v)
	{
		// handle the case when v has nan because step_coords caused a divde-by-zero
		if (v.x <= 0 || v.x != v.x)
			v.x = 1000000000;
		if (v.y <= 0 || v.y != v.y)
			v.y = 1000000000;
		if (v.z <= 0 || v.z != v.z)
			v.z = 1000000000;
		
		if (v.x <= v.y && v.x <= v.z)
			return v.x;
		if (v.y <= v.z && v.y <= v.x)
			return v.y;
		else
			return v.z;
	};
	
	// if t is nan, just make it 0
	if (t.x != t.x)
		t.x = 0.0f;
	if (t.y != t.y)
		t.y = 0.0f;
	if (t.z != t.z)
		t.z = 0.0f;

	time = min(t);

	// clamp time to 0 and 1
	if (time >= 1.0f)
	{
		time = 1.0f;
	}
	if (time <= 0.0f)
	{
		time = 0.0f;
	}

	assert(time == time);
	end.weights = start.weights + (step_coords * time);
	
	// reorder the indices if needed
	if (time != 1.0f)
	{
		if (end.weights.x <= 0.0001f)
		{
			end.weights = glm::vec3(end.weights.y, end.weights.z, 0.0f);
			end.indices = glm::uvec3(end.indices.y, end.indices.z, end.indices.x);
		}
		else if (end.weights.y <= 0.0001f )
		{
			end.weights = glm::vec3(end.weights.z, end.weights.x, 0.0f);
			end.indices = glm::uvec3(end.indices.z, end.indices.x, end.indices.y);
		}
		else if (end.weights.z <= 0.0001f)
			end.weights.z = 0.0f;
	}

	if (time != 1.0f)
	{
		assert(end.weights.z == 0.0f);
	} else {
		if (!(end.weights.x > 0 && end.weights.y > 0 && end.weights.z > 0))
		{
			if (end.weights.x <= 0)
				end.weights.x = 0.f;
			if (end.weights.y <= 0)
				end.weights.y = 0.f;
			if (end.weights.z <= 0)
				end.weights.z = 0.f;
		}
	}

	// clamp weights to avoid flying all over walkmesh
	if (end.weights.x > 1.0f)
		end.weights.x = 1.0f;
	if (end.weights.y > 1.0f)
		end.weights.y = 1.0f;
	if (end.weights.z > 1.0f)
		end.weights.z = 1.0f;
	if (end.weights.x < 0.0f)
		end.weights.x = 0.0f;
	if (end.weights.y < 0.0f)
		end.weights.y = 0.0f;
	if (end.weights.z < 0.0f)
		end.weights.z = 0.0f;

	assert(time <= 1.00001f && 0.0f <= time);
	return;
}

bool WalkMesh::cross_edge(WalkPoint const &start, WalkPoint *end_, glm::quat *rotation_) const {
	assert(end_);
	auto &end = *end_;

	assert(rotation_);
	auto &rotation = *rotation_;

	assert(start.weights.z == 0.0f); //*must* be on an edge.
    auto v = next_vertex.find(glm::uvec2(start.indices.x, start.indices.y));

	//check if 'edge' is a non-boundary edge:
	if (v != next_vertex.end()) {
        //it is!
		glm::uvec3 tri(start.indices.y, start.indices.x, v->second);

		//make 'end' represent the same (world) point, but on triangle (edge.y, edge.x, [other point]):
		end.indices = tri;
        end.weights = glm::vec3(start.weights.y, start.weights.x, 0.0f);
		assert(end.weights.x == end.weights.x);
		assert(end.weights.y == end.weights.y);
		assert(end.weights.z == end.weights.z);

		//make 'rotation' the rotation that takes (start.indices)'s normal to (end.indices)'s normal:
		glm::vec3 const &a = vertices[start.indices.x];
        glm::vec3 const &b = vertices[start.indices.y];
        glm::vec3 const &c = vertices[start.indices.z];
		assert(0 <= v->second && v->second < vertices.size());
        glm::vec3 const &d = vertices[v->second];
		assert(d.x == d.x);
		assert(d.y == d.y);
		assert(d.z == d.z);

		glm::vec3 c1 = glm::cross(b - a, c - a);
		assert (c1.x == c1.x);
		assert (c1.y == c1.y);
		assert (c1.z == c1.z);
		
		glm::vec3 c2 = glm::cross(b - c, d - b);
		assert(c2.x == c2.x);
		assert(c2.y == c2.y);
		assert(c2.z == c2.z);

        glm::vec3 n1 = c1 == glm::vec3(0, 0, 0) ? c1 : glm::normalize(c1);
		assert(n1.x == n1.x);
		assert(n1.y == n1.y);
		assert(n1.z == n1.z);

        glm::vec3 n2 = c2 == glm::vec3(0, 0, 0) ? c2 : glm::normalize(c2);
		assert(n2.x == n2.x);
		assert(n2.y == n2.y);
		assert(n2.z == n2.z);

        rotation = glm::rotation(n1, n2);

		return true; 
	} else {
		end = start;
		rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		return false;
	}
}


WalkMeshes::WalkMeshes(std::string const &filename) {
	std::ifstream file(filename, std::ios::binary);

	std::vector< glm::vec3 > vertices;
	read_chunk(file, "p...", &vertices);

	std::vector< glm::vec3 > normals;
	read_chunk(file, "n...", &normals);

	std::vector< glm::uvec3 > triangles;
	read_chunk(file, "tri0", &triangles);

	std::vector< char > names;
	read_chunk(file, "str0", &names);

	struct IndexEntry {
		uint32_t name_begin, name_end;
		uint32_t vertex_begin, vertex_end;
		uint32_t triangle_begin, triangle_end;
	};

	std::vector< IndexEntry > index;
	read_chunk(file, "idxA", &index);

	if (file.peek() != EOF) {
		std::cerr << "WARNING: trailing data in walkmesh file '" << filename << "'" << std::endl;
	}

	//-----------------

	if (vertices.size() != normals.size()) {
		throw std::runtime_error("Mis-matched position and normal sizes in '" + filename + "'");
	}

	for (auto const &e : index) {
		if (!(e.name_begin <= e.name_end && e.name_end <= names.size())) {
			throw std::runtime_error("Invalid name indices in index of '" + filename + "'");
		}
		if (!(e.vertex_begin <= e.vertex_end && e.vertex_end <= vertices.size())) {
			throw std::runtime_error("Invalid vertex indices in index of '" + filename + "'");
		}
		if (!(e.triangle_begin <= e.triangle_end && e.triangle_end <= triangles.size())) {
			throw std::runtime_error("Invalid triangle indices in index of '" + filename + "'");
		}

		//copy vertices/normals:
		std::vector< glm::vec3 > wm_vertices(vertices.begin() + e.vertex_begin, vertices.begin() + e.vertex_end);
		std::vector< glm::vec3 > wm_normals(normals.begin() + e.vertex_begin, normals.begin() + e.vertex_end);

		//remap triangles:
		std::vector< glm::uvec3 > wm_triangles; wm_triangles.reserve(e.triangle_end - e.triangle_begin);
		for (uint32_t ti = e.triangle_begin; ti != e.triangle_end; ++ti) {
			if (!( (e.vertex_begin <= triangles[ti].x && triangles[ti].x < e.vertex_end)
			    && (e.vertex_begin <= triangles[ti].y && triangles[ti].y < e.vertex_end)
			    && (e.vertex_begin <= triangles[ti].z && triangles[ti].z < e.vertex_end) )) {
				throw std::runtime_error("Invalid triangle in '" + filename + "'");
			}
			wm_triangles.emplace_back(
				triangles[ti].x - e.vertex_begin,
				triangles[ti].y - e.vertex_begin,
				triangles[ti].z - e.vertex_begin
			);
		}
		
		std::string name(names.begin() + e.name_begin, names.begin() + e.name_end);

		auto ret = meshes.emplace(name, WalkMesh(wm_vertices, wm_normals, wm_triangles));
		if (!ret.second) {
			throw std::runtime_error("WalkMesh with duplicated name '" + name + "' in '" + filename + "'");
		}

	}
}

WalkMesh const &WalkMeshes::lookup(std::string const &name) const {
	auto f = meshes.find(name);
	if (f == meshes.end()) {
		throw std::runtime_error("WalkMesh with name '" + name + "' not found.");
	}
	return f->second;
}
