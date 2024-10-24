#include "common.h"

CMatrix::CMatrix(void)
{
	m_attachment = nil;
	m_hasRwMatrix = false;
}

CMatrix::CMatrix(CMatrix const &m)
{
	m_attachment = nil;
	m_hasRwMatrix = false;
	*this = m;
}

CMatrix::CMatrix(RwMatrix *matrix, bool owner)
{
	m_attachment = nil;
	Attach(matrix, owner);
}

CMatrix::~CMatrix(void)
{
	if (m_hasRwMatrix && m_attachment)
		RwMatrixDestroy(m_attachment);
}

void
CMatrix::Attach(RwMatrix *matrix, bool owner)
{
#ifdef FIX_BUGS
	if (m_attachment && m_hasRwMatrix)
#else
	if (m_hasRwMatrix && m_attachment)
#endif
		RwMatrixDestroy(m_attachment);
	m_attachment = matrix;
	m_hasRwMatrix = owner;
	Update();
}

void
CMatrix::AttachRW(RwMatrix *matrix, bool owner)
{
	if (m_hasRwMatrix && m_attachment)
		RwMatrixDestroy(m_attachment);
	m_attachment = matrix;
	m_hasRwMatrix = owner;
	UpdateRW();
}

void
CMatrix::Detach(void)
{
	if (m_hasRwMatrix && m_attachment)
		RwMatrixDestroy(m_attachment);
	m_attachment = nil;
}

void
CMatrix::Update(void)
{
	GetRight() = m_attachment->right;
	GetForward() = m_attachment->up;
	GetUp() = m_attachment->at;
	GetPosition() = m_attachment->pos;
}

void
CMatrix::UpdateRW(void)
{
	if (m_attachment) {
		m_attachment->right = GetRight();
		m_attachment->up = GetForward();
		m_attachment->at = GetUp();
		m_attachment->pos = GetPosition();
		RwMatrixUpdate(m_attachment);
	}
}

void
CMatrix::operator=(CMatrix const &rhs)
{
	memcpy(this, &rhs, sizeof(f));
	if (m_attachment)
		UpdateRW();
}

void
CMatrix::CopyOnlyMatrix(const CMatrix &other)
{
	memcpy(this, &other, sizeof(f));
}

CMatrix &
CMatrix::operator+=(CMatrix const &rhs)
{
	GetRight() += rhs.GetRight();
	GetForward() += rhs.GetForward();
	GetUp() += rhs.GetUp();
	GetPosition() += rhs.GetPosition();
	return *this;
}

void
CMatrix::SetUnity(void)
{
	rx = 1.0f;
	ry = 0.0f;
	rz = 0.0f;
	fx = 0.0f;
	fy = 1.0f;
	fz = 0.0f;
	ux = 0.0f;
	uy = 0.0f;
	uz = 1.0f;
	px = 0.0f;
	py = 0.0f;
	pz = 0.0f;
}

void
CMatrix::ResetOrientation(void)
{
	rx = 1.0f;
	ry = 0.0f;
	rz = 0.0f;
	fx = 0.0f;
	fy = 1.0f;
	fz = 0.0f;
	ux = 0.0f;
	uy = 0.0f;
	uz = 1.0f;
}

void
CMatrix::SetScale(float s)
{
	rx = s;
	ry = 0.0f;
	rz = 0.0f;

	fx = 0.0f;
	fy = s;
	fz = 0.0f;

	ux = 0.0f;
	uy = 0.0f;
	uz = s;

	px = 0.0f;
	py = 0.0f;
	pz = 0.0f;
}

void
CMatrix::SetTranslate(float x, float y, float z)
{
	rx = 1.0f;
	ry = 0.0f;
	rz = 0.0f;

	fx = 0.0f;
	fy = 1.0f;
	fz = 0.0f;

	ux = 0.0f;
	uy = 0.0f;
	uz = 1.0f;

	px = x;
	py = y;
	pz = z;
}

void
CMatrix::SetRotateXOnly(float angle)
{
#ifdef PSP2
	float cs[2];
	sincosf_c(angle, cs);
#else
	float c = Cos(angle);
	float s = Sin(angle);
#endif
	rx = 1.0f;
	ry = 0.0f;
	rz = 0.0f;

	fx = 0.0f;
#ifdef PSP2
	fy = cs[1];
	fz = cs[0];

	ux = 0.0f;
	uy = -cs[0];
	uz = cs[1];
#else
	fy = c;
	fz = s;

	ux = 0.0f;
	uy = -s;
	ux = c;
#endif
}

void
CMatrix::SetRotateYOnly(float angle)
{
#ifdef PSP2
	float cs[2];
	sincosf_c(angle, cs);

	rx = cs[1];
	ry = 0.0f;
	rz = -cs[0];
#else
	float c = Cos(angle);
	float s = Sin(angle);

	rx = c;
	ry = 0.0f;
	rz = -s;
#endif
	fx = 0.0f;
	fy = 1.0f;
	fz = 0.0f;
#ifdef PSP2
	ux = cs[0];
	uy = 0.0f;
	uz = cs[1];
#else
	ux = s;
	uy = 0.0f;
	uz = c;
#endif
}

void
CMatrix::SetRotateZOnly(float angle)
{
#ifdef PSP2
	float cs[2];
	sincosf_c(angle, cs);

	rx = cs[1];
	ry = cs[0];
	rz = 0.0f;

	fx = -cs[0];
	fy = cs[1];
	fz = 0.0f;
#else
	float c = Cos(angle);
	float s = Sin(angle);

	rx = c;
	ry = s;
	rz = 0.0f;

	fx = -s;
	fy = c;
	fz = 0.0f;
#endif
	ux = 0.0f;
	uy = 0.0f;
	uz = 1.0f;
}

void
CMatrix::SetRotateX(float angle)
{
	SetRotateXOnly(angle);
	px = 0.0f;
	py = 0.0f;
	pz = 0.0f;
}


void
CMatrix::SetRotateY(float angle)
{
	SetRotateYOnly(angle);
	px = 0.0f;
	py = 0.0f;
	pz = 0.0f;
}

void
CMatrix::SetRotateZ(float angle)
{
	SetRotateZOnly(angle);
	px = 0.0f;
	py = 0.0f;
	pz = 0.0f;
}

void
CMatrix::SetRotate(float xAngle, float yAngle, float zAngle)
{
#ifdef PSP2
	float csX[2];
	float csY[2];
	float csZ[2];
	sincosf_c(xAngle, csX);
	sincosf_c(yAngle, csY);
	sincosf_c(zAngle, csZ);

	rx = csZ[1] * csY[1] - (csZ[0] * csX[0]) * csY[0];
	ry = (csZ[1] * csX[0]) * csY[0] + csZ[0] * csY[1];
	rz = -csX[1] * csY[0];

	fx = -csZ[0] * csX[1];
	fy = csZ[1] * csX[1];
	fz = csX[0];

	ux = (csZ[0] * csX[0]) * csY[1] + csZ[1] * csY[0];
	uy = csZ[0] * csY[0] - (csZ[1] * csX[0]) * csY[1];
	uz = csX[1] * csY[1];
#else
	float cX = Cos(xAngle);
	float sX = Sin(xAngle);
	float cY = Cos(yAngle);
	float sY = Sin(yAngle);
	float cZ = Cos(zAngle);
	float sZ = Sin(zAngle);

	rx = cZ * cY - (sZ * sX) * sY;
	ry = (cZ * sX) * sY + sZ * cY;
	rz = -cX * sY;

	fx = -sZ * cX;
	fy = cZ * cX;
	fz = sX;

	ux = (sZ * sX) * cY + cZ * sY;
	uy = sZ * sY - (cZ * sX) * cY;
	uz = cX * cY;
#endif

	px = 0.0f;
	py = 0.0f;
	pz = 0.0f;
}

void
CMatrix::RotateX(float x)
{
#ifdef PSP2
	float cs[2];
	sincosf_c(x, cs);
#else
	float c = Cos(x);
	float s = Sin(x);
#endif
	float ry = this->ry;
	float rz = this->rz;
	float uy = this->fy;
	float uz = this->fz;
	float ay = this->uy;
	float az = this->uz;
	float py = this->py;
	float pz = this->pz;
#ifdef PSP2
	this->ry = cs[1] * ry - cs[0] * rz;
	this->rz = cs[1] * rz + cs[0] * ry;
	this->fy = cs[1] * uy - cs[0] * uz;
	this->fz = cs[1] * uz + cs[0] * uy;
	this->uy = cs[1] * ay - cs[0] * az;
	this->uz = cs[1] * az + cs[0] * ay;
	this->py = cs[1] * py - cs[0] * pz;
	this->pz = cs[1] * pz + cs[0] * py;
#else
	this->ry = c * ry - s * rz;
	this->rz = c * rz + s * ry;
	this->fy = c * uy - s * uz;
	this->fz = c * uz + s * uy;
	this->uy = c * ay - s * az;
	this->uz = c * az + s * ay;
	this->py = c * py - s * pz;
	this->pz = c * pz + s * py;
#endif
}

void
CMatrix::RotateY(float y)
{
#ifdef PSP2
	float cs[2];
	sincosf_c(y, cs);
#else
	float c = Cos(y);
	float s = Sin(y);
#endif
	float rx = this->rx;
	float rz = this->rz;
	float ux = this->fx;
	float uz = this->fz;
	float ax = this->ux;
	float az = this->uz;
	float px = this->px;
	float pz = this->pz;
#ifdef PSP2
	this->rx = cs[1] * rx + cs[0] * rz;
	this->rz = cs[1] * rz - cs[0] * rx;
	this->fx = cs[1] * ux + cs[0] * uz;
	this->fz = cs[1] * uz - cs[0] * ux;
	this->ux = cs[1] * ax + cs[0] * az;
	this->uz = cs[1] * az - cs[0] * ax;
	this->px = cs[1] * px + cs[0] * pz;
	this->pz = cs[1] * pz - cs[0] * px;
#else
	this->rx = c * rx + s * rz;
	this->rz = c * rz - s * rx;
	this->fx = c * ux + s * uz;
	this->fz = c * uz - s * ux;
	this->ux = c * ax + s * az;
	this->uz = c * az - s * ax;
	this->px = c * px + s * pz;
	this->pz = c * pz - s * px;
#endif
}

void
CMatrix::RotateZ(float z)
{
#ifdef PSP2
	float cs[2];
	sincosf_c(z, cs);
#else
	float c = Cos(z);
	float s = Sin(z);
#endif
	float ry = this->ry;
	float rx = this->rx;
	float uy = this->fy;
	float ux = this->fx;
	float ay = this->uy;
	float ax = this->ux;
	float py = this->py;
	float px = this->px;
#ifdef PSP2
	this->rx = cs[1] * rx - cs[0] * ry;
	this->ry = cs[1] * ry + cs[0] * rx;
	this->fx = cs[1] * ux - cs[0] * uy;
	this->fy = cs[1] * uy + cs[0] * ux;
	this->ux = cs[1] * ax - cs[0] * ay;
	this->uy = cs[1] * ay + cs[0] * ax;
	this->px = cs[1] * px - cs[0] * py;
	this->py = cs[1] * py + cs[0] * px;
#else
	this->rx = c * rx - s * ry;
	this->ry = c * ry + s * rx;
	this->fx = c * ux - s * uy;
	this->fy = c * uy + s * ux;
	this->ux = c * ax - s * ay;
	this->uy = c * ay + s * ax;
	this->px = c * px - s * py;
	this->py = c * py + s * px;
#endif
}

void
CMatrix::Rotate(float x, float y, float z)
{
#ifdef PSP2
	float csX[2];
	float csY[2];
	float csZ[2];
	sincosf_c(x, csX);
	sincosf_c(y, csY);
	sincosf_c(z, csZ);
#else
	float cX = Cos(x);
	float sX = Sin(x);
	float cY = Cos(y);
	float sY = Sin(y);
	float cZ = Cos(z);
	float sZ = Sin(z);
#endif
	float rx = this->rx;
	float ry = this->ry;
	float rz = this->rz;
	float ux = this->fx;
	float uy = this->fy;
	float uz = this->fz;
	float ax = this->ux;
	float ay = this->uy;
	float az = this->uz;
	float px = this->px;
	float py = this->py;
	float pz = this->pz;
#ifdef PSP2
	float x1 = csZ[1] * csY[1] - (csZ[0] * csX[0]) * csY[0];
	float x2 = (csZ[1] * csX[0]) * csY[0] + csZ[0] * csY[1];
	float x3 = -csX[1] * csY[0];
	float y1 = -csZ[0] * csX[1];
	float y2 = csZ[1] * csX[1];
	float y3 = csX[0];
	float z1 = (csZ[0] * csX[0]) * csY[1] + csZ[1] * csY[0];
	float z2 = csZ[0] * csY[0] - (csZ[1] * csX[0]) * csY[1];
	float z3 = csX[1] * csY[1];
#else
	float x1 = cZ * cY - (sZ * sX) * sY;
	float x2 = (cZ * sX) * sY + sZ * cY;
	float x3 = -cX * sY;
	float y1 = -sZ * cX;
	float y2 = cZ * cX;
	float y3 = sX;
	float z1 = (sZ * sX) * cY + cZ * sY;
	float z2 = sZ * sY - (cZ * sX) * cY;
	float z3 = cX * cY;
#endif

	this->rx = x1 * rx + y1 * ry + z1 * rz;
	this->ry = x2 * rx + y2 * ry + z2 * rz;
	this->rz = x3 * rx + y3 * ry + z3 * rz;
	this->fx = x1 * ux + y1 * uy + z1 * uz;
	this->fy = x2 * ux + y2 * uy + z2 * uz;
	this->fz = x3 * ux + y3 * uy + z3 * uz;
	this->ux = x1 * ax + y1 * ay + z1 * az;
	this->uy = x2 * ax + y2 * ay + z2 * az;
	this->uz = x3 * ax + y3 * ay + z3 * az;
	this->px = x1 * px + y1 * py + z1 * pz;
	this->py = x2 * px + y2 * py + z2 * pz;
	this->pz = x3 * px + y3 * py + z3 * pz;
}

CMatrix &
CMatrix::operator*=(CMatrix const &rhs)
{
	// TODO: VU0 code
	*this = *this * rhs;
	return *this;
}

void
CMatrix::Reorthogonalise(void)
{
	CVector &r = GetRight();
	CVector &f = GetForward();
	CVector &u = GetUp();
	u = CrossProduct(r, f);
	u.Normalise();
	r = CrossProduct(f, u);
	r.Normalise();
	f = CrossProduct(u, r);
}

CMatrix
operator*(const CMatrix &m1, const CMatrix &m2)
{
	// TODO: VU0 code
	CMatrix out;

	out.rx = m1.rx * m2.rx + m1.fx * m2.ry + m1.ux * m2.rz;
	out.ry = m1.ry * m2.rx + m1.fy * m2.ry + m1.uy * m2.rz;
	out.rz = m1.rz * m2.rx + m1.fz * m2.ry + m1.uz * m2.rz;
	out.fx = m1.rx * m2.fx + m1.fx * m2.fy + m1.ux * m2.fz;
	out.fy = m1.ry * m2.fx + m1.fy * m2.fy + m1.uy * m2.fz;
	out.fz = m1.rz * m2.fx + m1.fz * m2.fy + m1.uz * m2.fz;
	out.ux = m1.rx * m2.ux + m1.fx * m2.uy + m1.ux * m2.uz;
	out.uy = m1.ry * m2.ux + m1.fy * m2.uy + m1.uy * m2.uz;
	out.uz = m1.rz * m2.ux + m1.fz * m2.uy + m1.uz * m2.uz;
	out.px = m1.rx * m2.px + m1.fx * m2.py + m1.ux * m2.pz + m1.px;
	out.py = m1.ry * m2.px + m1.fy * m2.py + m1.uy * m2.pz + m1.py;
	out.pz = m1.rz * m2.px + m1.fz * m2.py + m1.uz * m2.pz + m1.pz;
	return out;
}

CMatrix &
Invert(const CMatrix &src, CMatrix &dst)
{
	// TODO: VU0 code
	dst.f[3][0] = dst.f[3][1] = dst.f[3][2] = 0.0f;

	dst.f[0][0] = src.f[0][0];
	dst.f[0][1] = src.f[1][0];
	dst.f[0][2] = src.f[2][0];

	dst.f[1][0] = src.f[0][1];
	dst.f[1][1] = src.f[1][1];
	dst.f[1][2] = src.f[2][1];

	dst.f[2][0] = src.f[0][2];
	dst.f[2][1] = src.f[1][2];
	dst.f[2][2] = src.f[2][2];


	dst.f[3][0] += dst.f[0][0] * src.f[3][0];
	dst.f[3][1] += dst.f[0][1] * src.f[3][0];
	dst.f[3][2] += dst.f[0][2] * src.f[3][0];

	dst.f[3][0] += dst.f[1][0] * src.f[3][1];
	dst.f[3][1] += dst.f[1][1] * src.f[3][1];
	dst.f[3][2] += dst.f[1][2] * src.f[3][1];

	dst.f[3][0] += dst.f[2][0] * src.f[3][2];
	dst.f[3][1] += dst.f[2][1] * src.f[3][2];
	dst.f[3][2] += dst.f[2][2] * src.f[3][2];

	dst.f[3][0] = -dst.f[3][0];
	dst.f[3][1] = -dst.f[3][1];
	dst.f[3][2] = -dst.f[3][2];

	return dst;
}

void
CMatrix::CopyToRwMatrix(RwMatrix* matrix)
{
	matrix->right = GetRight();
	matrix->up = GetForward();
	matrix->at = GetUp();
	matrix->pos = GetPosition();
	RwMatrixUpdate(matrix);
}

CMatrix
Invert(const CMatrix &matrix)
{
	CMatrix inv;
	return Invert(matrix, inv);
}

void
CCompressedMatrixNotAligned::CompressFromFullMatrix(CMatrix &other)
{
	m_rightX = 127.0f * other.GetRight().x;
	m_rightY = 127.0f * other.GetRight().y;
	m_rightZ = 127.0f * other.GetRight().z;
	m_upX = 127.0f * other.GetForward().x;
	m_upY = 127.0f * other.GetForward().y;
	m_upZ = 127.0f * other.GetForward().z;
	m_vecPos = other.GetPosition();
}

void
CCompressedMatrixNotAligned::DecompressIntoFullMatrix(CMatrix &other)
{
	other.GetRight().x = m_rightX / 127.0f;
	other.GetRight().y = m_rightY / 127.0f;
	other.GetRight().z = m_rightZ / 127.0f;
	other.GetForward().x = m_upX / 127.0f;
	other.GetForward().y = m_upY / 127.0f;
	other.GetForward().z = m_upZ / 127.0f;
	other.GetUp() = CrossProduct(other.GetRight(), other.GetForward());
	other.GetPosition() = m_vecPos;
	other.Reorthogonalise();
}