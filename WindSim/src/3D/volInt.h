#ifndef VOL_INT_H
#define VOL_INT_H

/*******************************************************
*                                                      *
*  volInt.c                                            *
*                                                      *
*  This code computes volume integrals needed for      *
*  determining mass properties of polyhedral bodies.   *
*                                                      *
*  For more information, see the accompanying README   *
*  file, and the paper                                 *
*                                                      *
*  Brian Mirtich, "Fast and Accurate Computation of    *
*  Polyhedral Mass Properties," journal of graphics    *
*  tools, volume 1, number 1, 1996.                    *
*                                                      *
*  This source code is public domain, and may be used  *
*  in any way, shape or form, free of charge.          *
*                                                      *
*  Copyright 1995 by Brian Mirtich                     *
*                                                      *
*  mirtich@cs.berkeley.edu                             *
*  http://www.cs.berkeley.edu/~mirtich                 *
*                                                      *
*******************************************************/

/*
===== Modified 11 Feb 2016 =====
*/

#include <cmath>
#include <vector>
#include <array>
#include <cstdint>

namespace VolInt
{

/*
============================================================================
constants
============================================================================
*/

	static int X = 0;
	static int Y = 1;
	static int Z = 2;

/*
============================================================================
macros
============================================================================
*/

#define SQR(x) ((x)*(x))
#define CUBE(x) ((x)*(x)*(x))


	/*
	============================================================================
	data structures
	============================================================================
	*/
	struct POLYHEDRON;

	struct FACE
	{
		float norm[3];
		float w;
		int verts[3];
		POLYHEDRON *poly;
	};

	struct POLYHEDRON
	{
		int numVerts, numFaces;
		std::vector<std::array<float, 3>> verts;
		std::vector<FACE> faces;
	};


	/*
	============================================================================
	globals
	============================================================================
	*/

	static int A;   /* alpha */
	static int B;   /* beta */
	static int C;   /* gamma */

	/* projection integrals */
	static double P1, Pa, Pb, Paa, Pab, Pbb, Paaa, Paab, Pabb, Pbbb;

	/* face integrals */
	static double Fa, Fb, Fc, Faa, Fbb, Fcc, Faaa, Fbbb, Fccc, Faab, Fbbc, Fcca;

	/* volume integrals */
	static double T0, T1[3], T2[3], TP[3];


	/*
	============================================================================
	read in a polyhedron
	============================================================================
	*/

	static void readPolyhedron(const std::vector<uint32_t>& indexBuffer, const std::vector<float>& vertexBuffer, const std::vector<float>& scaling, POLYHEDRON *p)
	{
		int i, j, n;
		double dx1, dy1, dz1, dx2, dy2, dz2, nx, ny, nz, len;
		FACE *f;

		p->numVerts = vertexBuffer.size() / 6; // 3 float position, 3 float normal
		std::array<float, 3> a = { 0.0f, 0.0f, 0.0f };
		p->verts.resize(p->numVerts, a);

		for (i = 0; i < p->numVerts; i++)
		{
			p->verts[i][X] = vertexBuffer[i * 6 + X] * scaling[X];
			p->verts[i][Y] = vertexBuffer[i * 6 + Y] * scaling[Y];
			p->verts[i][Z] = -vertexBuffer[i * 6 + Z] * scaling[Z]; // DirectX uses left-handed system
		}

		p->numFaces = indexBuffer.size() / 3;
		p->faces.resize(p->numFaces);

		for (i = 0; i < p->numFaces; i++)
		{
			f = &p->faces[i];
			f->poly = p;

			// Input is clock-wise -> modify to counter-clockwise for algorithm
			f->verts[X] = indexBuffer[i * 3 + Z];
			f->verts[Y] = indexBuffer[i * 3 + Y];
			f->verts[Z] = indexBuffer[i * 3 + X];

			/* compute face normal and offset w from first 3 vertices */
			dx1 = p->verts[f->verts[1]][X] - p->verts[f->verts[0]][X];
			dy1 = p->verts[f->verts[1]][Y] - p->verts[f->verts[0]][Y];
			dz1 = p->verts[f->verts[1]][Z] - p->verts[f->verts[0]][Z];
			dx2 = p->verts[f->verts[2]][X] - p->verts[f->verts[1]][X];
			dy2 = p->verts[f->verts[2]][Y] - p->verts[f->verts[1]][Y];
			dz2 = p->verts[f->verts[2]][Z] - p->verts[f->verts[1]][Z];
			nx = dy1 * dz2 - dy2 * dz1;
			ny = dz1 * dx2 - dz2 * dx1;
			nz = dx1 * dy2 - dx2 * dy1;
			len = sqrt(nx * nx + ny * ny + nz * nz);
			f->norm[X] = nx / len;
			f->norm[Y] = ny / len;
			f->norm[Z] = nz / len;
			f->w = -f->norm[X] * p->verts[f->verts[0]][X]
				- f->norm[Y] * p->verts[f->verts[0]][Y]
				- f->norm[Z] * p->verts[f->verts[0]][Z];

		}
	}

	/*
	============================================================================
	compute mass properties
	============================================================================
	*/


	/* compute various integrations over projection of face */
	static void compProjectionIntegrals(FACE *f)
	{
		double a0, a1, da;
		double b0, b1, db;
		double a0_2, a0_3, a0_4, b0_2, b0_3, b0_4;
		double a1_2, a1_3, b1_2, b1_3;
		double C1, Ca, Caa, Caaa, Cb, Cbb, Cbbb;
		double Cab, Kab, Caab, Kaab, Cabb, Kabb;
		int i;

		P1 = Pa = Pb = Paa = Pab = Pbb = Paaa = Paab = Pabb = Pbbb = 0.0;

		for (i = 0; i < 3; i++) {
			a0 = f->poly->verts[f->verts[i]][A];
			b0 = f->poly->verts[f->verts[i]][B];
			a1 = f->poly->verts[f->verts[(i + 1) % 3]][A];
			b1 = f->poly->verts[f->verts[(i + 1) % 3]][B];
			da = a1 - a0;
			db = b1 - b0;
			a0_2 = a0 * a0; a0_3 = a0_2 * a0; a0_4 = a0_3 * a0;
			b0_2 = b0 * b0; b0_3 = b0_2 * b0; b0_4 = b0_3 * b0;
			a1_2 = a1 * a1; a1_3 = a1_2 * a1;
			b1_2 = b1 * b1; b1_3 = b1_2 * b1;

			C1 = a1 + a0;
			Ca = a1*C1 + a0_2; Caa = a1*Ca + a0_3; Caaa = a1*Caa + a0_4;
			Cb = b1*(b1 + b0) + b0_2; Cbb = b1*Cb + b0_3; Cbbb = b1*Cbb + b0_4;
			Cab = 3 * a1_2 + 2 * a1*a0 + a0_2; Kab = a1_2 + 2 * a1*a0 + 3 * a0_2;
			Caab = a0*Cab + 4 * a1_3; Kaab = a1*Kab + 4 * a0_3;
			Cabb = 4 * b1_3 + 3 * b1_2*b0 + 2 * b1*b0_2 + b0_3;
			Kabb = b1_3 + 2 * b1_2*b0 + 3 * b1*b0_2 + 4 * b0_3;

			P1 += db*C1;
			Pa += db*Ca;
			Paa += db*Caa;
			Paaa += db*Caaa;
			Pb += da*Cb;
			Pbb += da*Cbb;
			Pbbb += da*Cbbb;
			Pab += db*(b1*Cab + b0*Kab);
			Paab += db*(b1*Caab + b0*Kaab);
			Pabb += da*(a1*Cabb + a0*Kabb);
		}

		P1 /= 2.0;
		Pa /= 6.0;
		Paa /= 12.0;
		Paaa /= 20.0;
		Pb /= -6.0;
		Pbb /= -12.0;
		Pbbb /= -20.0;
		Pab /= 24.0;
		Paab /= 60.0;
		Pabb /= -60.0;
	}

	static void compFaceIntegrals(FACE *f)
	{
		float *n, w;
		double k1, k2, k3, k4;

		compProjectionIntegrals(f);

		w = f->w;
		n = f->norm;
		k1 = 1 / n[C]; k2 = k1 * k1; k3 = k2 * k1; k4 = k3 * k1;

		Fa = k1 * Pa;
		Fb = k1 * Pb;
		Fc = -k2 * (n[A] * Pa + n[B] * Pb + w*P1);

		Faa = k1 * Paa;
		Fbb = k1 * Pbb;
		Fcc = k3 * (SQR(n[A])*Paa + 2 * n[A] * n[B] * Pab + SQR(n[B])*Pbb
			+ w*(2 * (n[A] * Pa + n[B] * Pb) + w*P1));

		Faaa = k1 * Paaa;
		Fbbb = k1 * Pbbb;
		Fccc = -k4 * (CUBE(n[A])*Paaa + 3 * SQR(n[A])*n[B] * Paab
			+ 3 * n[A] * SQR(n[B])*Pabb + CUBE(n[B])*Pbbb
			+ 3 * w*(SQR(n[A])*Paa + 2 * n[A] * n[B] * Pab + SQR(n[B])*Pbb)
			+ w*w*(3 * (n[A] * Pa + n[B] * Pb) + w*P1));

		Faab = k1 * Paab;
		Fbbc = -k2 * (n[A] * Pabb + n[B] * Pbbb + w*Pbb);
		Fcca = k3 * (SQR(n[A])*Paaa + 2 * n[A] * n[B] * Paab + SQR(n[B])*Pabb
			+ w*(2 * (n[A] * Paa + n[B] * Pab) + w*Pa));
	}

	static void compVolumeIntegrals(POLYHEDRON *p)
	{
		FACE *f;
		double nx, ny, nz;
		int i;

		T0 = T1[X] = T1[Y] = T1[Z]
			= T2[X] = T2[Y] = T2[Z]
			= TP[X] = TP[Y] = TP[Z] = 0;

		for (i = 0; i < p->numFaces; i++) {

			f = &p->faces[i];

			nx = fabs(f->norm[X]);
			ny = fabs(f->norm[Y]);
			nz = fabs(f->norm[Z]);
			if (nx > ny && nx > nz) C = X;
			else C = (ny > nz) ? Y : Z;
			A = (C + 1) % 3;
			B = (A + 1) % 3;

			compFaceIntegrals(f);

			T0 += f->norm[X] * ((A == X) ? Fa : ((B == X) ? Fb : Fc));

			T1[A] += f->norm[A] * Faa;
			T1[B] += f->norm[B] * Fbb;
			T1[C] += f->norm[C] * Fcc;
			T2[A] += f->norm[A] * Faaa;
			T2[B] += f->norm[B] * Fbbb;
			T2[C] += f->norm[C] * Fccc;
			TP[A] += f->norm[A] * Faab;
			TP[B] += f->norm[B] * Fbbc;
			TP[C] += f->norm[C] * Fcca;
		}

		T1[X] /= 2; T1[Y] /= 2; T1[Z] /= 2;
		T2[X] /= 3; T2[Y] /= 3; T2[Z] /= 3;
		TP[X] /= 2; TP[Y] /= 2; TP[Z] /= 2;
	}


	/*
	============================================================================
	main
	============================================================================
	*/


	static void calcMassProps(const std::vector<uint32_t>& indexBuffer, const std::vector<float>& vertexBuffer, const std::vector<float>& scaling, const float density, std::vector<float>& inertiaTensor, std::vector<float>& centerOfMass, float* mass, float* volume)
	{
		POLYHEDRON p;
		float m;
		std::vector<float>& r = centerOfMass;
		std::vector<float>& J = inertiaTensor;
		r.resize(3, 0);            /* center of mass */
		J.resize(3 * 3, 0);         /* inertia tensor */

		readPolyhedron(indexBuffer, vertexBuffer, scaling, &p);

		compVolumeIntegrals(&p);

		printf("\nT1 =   %+20.6f\n\n", T0);

		printf("Tx =   %+20.6f\n", T1[X]);
		printf("Ty =   %+20.6f\n", T1[Y]);
		printf("Tz =   %+20.6f\n\n", T1[Z]);

		printf("Txx =  %+20.6f\n", T2[X]);
		printf("Tyy =  %+20.6f\n", T2[Y]);
		printf("Tzz =  %+20.6f\n\n", T2[Z]);

		printf("Txy =  %+20.6f\n", TP[X]);
		printf("Tyz =  %+20.6f\n", TP[Y]);
		printf("Tzx =  %+20.6f\n\n", TP[Z]);

		m = density * T0;

		if (mass)
			*mass = m;

		if (volume)
			*volume = T0;

		/* compute center of mass */
		r[X] = T1[X] / T0;
		r[Y] = T1[Y] / T0;
		r[Z] = T1[Z] / T0;

		/* compute inertia tensor */
		J[X * 3 + X] = density * (T2[Y] + T2[Z]);
		J[Y * 3 + Y] = density * (T2[Z] + T2[X]);
		J[Z * 3 + Z] = density * (T2[X] + T2[Y]);
		J[X * 3 + Y] = J[Y * 3 + X] = -density * TP[X];
		J[Y * 3 + Z] = J[Z * 3 + Y] = -density * TP[Y];
		J[Z * 3 + X] = J[X * 3 + Z] = -density * TP[Z];

		/* translate inertia tensor to center of mass */
		J[X * 3 + X] -= m * (r[Y] * r[Y] + r[Z] * r[Z]);
		J[Y * 3 + Y] -= m * (r[Z] * r[Z] + r[X] * r[X]);
		J[Z * 3 + Z] -= m * (r[X] * r[X] + r[Y] * r[Y]);
		J[X * 3 + Y] = J[Y * 3 + X] += m * r[X] * r[Y];
		J[Y * 3 + Z] = J[Z * 3 + Y] += m * r[Y] * r[Z];
		J[Z * 3 + X] = J[X * 3 + Z] += m * r[Z] * r[X];

		// DirectX use left-handed coordinate system: com and inertia tensor are now in right-handed system -> flip Z
		r[Z] = -r[Z];
		J[X * 3 + Z] = -J[X * 3 + Z];
		J[Y * 3 + Z] = -J[Y * 3 + Z];
		J[Z * 3 + Z] = -J[Z * 3 + Z];

		printf("center of mass:  (%+12.6f,%+12.6f,%+12.6f)\n\n", r[X], r[Y], r[Z]);

		printf("inertia tensor with origin at c.o.m. :\n");
		printf("%+15.6f  %+15.6f  %+15.6f\n", J[X * 3 + X], J[X * 3 + Y], J[X * 3 + Z]);
		printf("%+15.6f  %+15.6f  %+15.6f\n", J[Y * 3 + X], J[Y * 3 + Y], J[Y * 3 + Z]);
		printf("%+15.6f  %+15.6f  %+15.6f\n\n", J[Z * 3 + X], J[Z * 3 + Y], J[Z * 3 + Z]);

	}
}

#endif