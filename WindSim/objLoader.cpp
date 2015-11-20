#include "objLoader.h"

using namespace objLoader;


bool ObjLoader::loadObj(const std::string path, std::vector<float>& vertexData, std::vector<uint32_t>& indexData)
{
	//Reset the buffers
	vertexData.clear();
	indexData.clear();


	std::vector<Vec3> position = std::vector<Vec3>();
	std::vector<Vec3> normal = std::vector<Vec3>();

	std::unordered_map<Vertex, int> vertexBuffer = std::unordered_map<Vertex, int>();

	std::string temp;

	std::ifstream in(path);
	if (in.fail())
	{
		return false;
	}
	while (in >> temp)
	{
		if (temp == "v") //Position
		{
			Vec3 p = { 0.0, 0.0, 0.0 };
			in >> p.x;
			in >> p.y;
			in >> p.z;
			p.z = -p.z; // Z-forward but obj is saved in -Z-forward
			position.push_back(p);
		}
		else if (temp == "vt") //Texture
		{
			float tu, tv = 0.0;
			in >> tu;
			in >> tv;
			// Ignore texture -> do not store
		}
		else if(temp == "vn") //Normal
		{
			Vec3 n = { 0.0, 0.0, 0.0 };
			in>> n.x;
			in>> n.y;
			in>> n.z;
			normal.push_back(n);
		}
		else if (temp == "f") // Face (Triangle or Quad) "f ip/[it]/in ip/[it]/in ip/[it]/in [ip/[it]/in]
		{
			uint32_t index;
			Vec3 p = { 0.0, 0.0, 0.0 };
			Vec3 n = { 0.0, 0.0, 0.0 };
			Vertex v = { p, n };
			for (int i = 0; i < 3; ++i)
			{
				in >> index;
				p = position[index - 1];
				v.p = p;
				if (in.peek() == '/') // It may follow a texture and normal index
				{
					in.ignore(); // Ignore '/'

					if (in.peek() != '/') // texture index is given
					{
						in >> index;
						// Ignore texture -> do not load texture coordinate
					}

					if (in.peek() == '/') // Normal is given
					{
						in.ignore();
						in >> index;
						n = normal[index - 1];
						v.n = n;
					}
				}

				//Insert vertex in vertex buffer
				auto it = vertexBuffer.find(v);
				if (it == vertexBuffer.end()) //Vertex is not yet in vertex buffer
				{
					uint32_t size = vertexBuffer.size();//Index of new last vertex is size of old buffer
					vertexBuffer[v] = size; //Add index for new vertex
					addVertexToBuffer(vertexData, v); //Add vertex to vertex buffer
					indexData.push_back(size); //Add new index to index buffer
				}
				else //Vertex already exists in vertexbuffer
				{
					indexData.push_back(it->second);
				}
			}
		}
	}//end while
	in.close();

	return true;
}


void ObjLoader::calculateNormals(std::vector<float>& vertexData, const std::vector<uint32_t>& indexData)
{
	int factor = sizeof(Vertex) / sizeof(float);
	std::vector<Vertex * const> vd;
	convertVertexData(vertexData, &vd);

	// Iterate all triangles
	for (auto it = indexData.begin(); it != indexData.end(); std::advance(it, 3))
	{
		// Load three vertices of current triangle
		std::vector<Vertex * const> v = { vd[*it], vd[*(it + 1)], vd[*(it + 2)] };
		Vec3 cross = Vec3::crossProduct(v[1]->p - v[0]->p, v[2]->p - v[0]->p);

		for (int j = 0; j < 3; ++j)
		{
			// Add weighted face normal to vertex normal
			Vec3 a = v[(j + 1) % 3]->p - v[j]->p;
			Vec3 b = v[(j + 2) % 3]->p - v[j]->p;
			float weight = acos(Vec3::dotProduct(a, b) / (a.length() * b.length()));
			v[j]->n += weight * cross;
		}

	}

	//Normalize normals
	std::vector<Vertex * const> v;
	convertVertexData(vertexData, &v);
	std::for_each(v.begin(), v.end(), [](Vertex* v){v->n.normalize();});
}


void ObjLoader::addVertexToBuffer(std::vector<float>& b, const Vertex& v)
{
	b.push_back(v.p.x);
	b.push_back(v.p.y);
	b.push_back(v.p.z);
	b.push_back(v.n.x);
	b.push_back(v.n.y);
	b.push_back(v.n.z);
}


//bool ObjLoader::save(const std::string path)
//{
//	viHeader h =
//	{
//		m_vertexBuffer.size(),
//		m_indexBuffer.size(),
//		m_vertexBuffer.size() * sizeof(vertex),
//		m_indexBuffer.size() * sizeof(uint32_t),
//		m_boundingSphereRadius,
//		m_boundingSphereCenter
//	};
//
//	std::ofstream out(path, std::ios_base::binary);
//	if (out.fail())
//	{
//		throw std::runtime_error("Error while saving file to path '" + path + "'!");
//		return false;
//	}
//	out.write(reinterpret_cast<char*>(&h), sizeof(viHeader));
//	out.write(reinterpret_cast<char*>(&m_vertexBuffer[0]), h.vertexBufferSize);
//	out.write(reinterpret_cast<char*>(&m_indexBuffer[0]), h.indexBufferSize);
//	if (out.fail())
//	{
//		throw std::runtime_error("Error while saving file to path '" + path + "'!");
//		return false;
//	}
//	out.close();
//	return true;
//}


float ObjLoader::normalizeSize(std::vector<float>& vertexData)
{
	std::vector<Vertex * const> vd;
	convertVertexData(vertexData, &vd);

	Vec3 center = findGeometricCenter(vd);

	//Translate all positions about center
	std::for_each(vd.begin(), vd.end(), [center](Vertex* v){v->p -= center;});

	//Find scale, which fits the component into a unit box
	float max = 0;
	for (auto it = vd.begin(); it != vd.end(); ++it)
	{
		if (abs((*it)->p.x) > max)
			max = abs((*it)->p.x);
		if (abs((*it)->p.y) > max)
			max = abs((*it)->p.y);
		if (abs((*it)->p.z) > max)
			max = abs((*it)->p.z);
	}
	// max^-1 is such scale
	std::for_each(vd.begin(), vd.end(), [max](Vertex* v){v->p = v->p * 1 / max;});
	return 1 / max;
}

void ObjLoader::findBoundingSphere(std::vector<float>& vertexData, std::vector<float>* bsCenter, float* bsRadius)
{
	std::vector<Vertex * const> vd;
	convertVertexData(vertexData, &vd);

	Vec3 center = findGeometricCenter(vd);
	bsCenter->clear();
	*bsCenter = { center.x, center.y, center.z };

	*bsRadius = (*std::max_element(vd.begin(), vd.end(), [](const Vertex*  const a, const Vertex*  const b){return a->p.lengthSquared() < b->p.lengthSquared();}))->p.length();
}


Vec3 ObjLoader::findGeometricCenter(const std::vector<Vertex * const>& b)
{
	//Find geometrical center
	Vec3 center = { 0.0, 0.0, 0.0 };
	return std::accumulate(b.begin(), b.end(), center, [](const Vec3& p, const Vertex* v){return p + v->p; }) / b.size();
}

void ObjLoader::convertVertexData(std::vector<float>& in, std::vector<Vertex * const>* out)
{
	// Get Vector of vertex pointer into the float vector:
	// Example:
	// Float vector of size 24 floats
	// Factor of Vertex/float is 6 (One vertex contains 6 floats)
	// => Vector contains 24 / 6 = 4 vertices
	//
	//  0       6      12      18    23
	// [******][******][******][******]
	//  ^       ^       ^       ^
	//  |       |       |       |
	//  0       1       2       3(24 / 6 - 1)

	int factor = sizeof(Vertex) / sizeof(float);
	float * f = in.data();
	Vertex * v = reinterpret_cast<Vertex *>(f);
	*out = std::vector<Vertex* const>(in.size() / factor);
	std::transform(v, std::next(v, in.size() / factor), out->begin(), [](Vertex& v){return &v; });
}