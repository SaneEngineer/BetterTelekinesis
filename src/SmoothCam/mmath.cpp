// Taken from https://github.com/mwilsnd/SkyrimSE-SmoothCam/blob/master/SmoothCam/source/mmath.cpp
#include "BetterTelekinesis/mmath.h"

bool mmath::IsInf(const float f) noexcept
{
	return glm::isinf(f);
}
bool mmath::IsInf(const double f) noexcept
{
	return glm::isinf(f);
}
bool mmath::IsInf(const glm::vec3& v) noexcept
{
	return glm::isinf(v.x) || glm::isinf(v.y) || glm::isinf(v.z);
}
bool mmath::IsInf(const glm::vec4& v) noexcept
{
	return glm::isinf(v.x) || glm::isinf(v.y) || glm::isinf(v.z) || glm::isinf(v.w);
}
bool mmath::IsInf(const glm::vec<4, float, glm::packed_highp>& v) noexcept
{
	return glm::isinf(v.x) || glm::isinf(v.y) || glm::isinf(v.z) || glm::isinf(v.w);
}

bool mmath::IsNan(const float f) noexcept
{
	return glm::isnan(f);
}
bool mmath::IsNan(const double f) noexcept
{
	return glm::isnan(f);
}
bool mmath::IsNan(const glm::vec3& v) noexcept
{
	return glm::isnan(v.x) || glm::isnan(v.y) || glm::isnan(v.z);
}
bool mmath::IsNan(const glm::vec4& v) noexcept
{
	return glm::isnan(v.x) || glm::isnan(v.y) || glm::isnan(v.z) || glm::isnan(v.w);
}
bool mmath::IsNan(const glm::vec<4, float, glm::packed_highp>& v) noexcept
{
	return glm::isnan(v.x) || glm::isnan(v.y) || glm::isnan(v.z) || glm::isnan(v.w);
}

bool mmath::IsValid(const float f) noexcept
{
	return !mmath::IsInf(f) && !mmath::IsNan(f);
}
bool mmath::IsValid(const double f) noexcept
{
	return !mmath::IsInf(f) && !mmath::IsNan(f);
}
bool mmath::IsValid(const glm::vec3& v) noexcept
{
	return !mmath::IsInf(v) && !mmath::IsNan(v);
}
bool mmath::IsValid(const glm::vec4& v) noexcept
{
	return !mmath::IsInf(v) && !mmath::IsNan(v);
}
bool mmath::IsValid(const glm::vec<4, float, glm::packed_highp>& v) noexcept
{
	return !mmath::IsInf(v) && !mmath::IsNan(v);
}
// Return the forward view vector
glm::vec3 mmath::GetViewVector(const glm::vec3& forwardRefer, float pitch, float yaw) noexcept
{
	auto aproxNormal = glm::vec4(forwardRefer.x, forwardRefer.y, forwardRefer.z, 1.0);

	auto m = glm::identity<glm::mat4>();
	m = glm::rotate(m, -pitch, glm::vec3(1.0f, 0.0f, 0.0f));
	aproxNormal = m * aproxNormal;

	m = glm::identity<glm::mat4>();
	m = glm::rotate(m, -yaw, glm::vec3(0.0f, 0.0f, 1.0f));
	aproxNormal = m * aproxNormal;

	return static_cast<glm::vec3>(aproxNormal);
}

glm::vec3 mmath::NiMatrixToEuler(const RE::NiMatrix3& m) noexcept
{
	glm::vec3 rot;
	const auto a = glm::clamp(-m.entry[2][0], -1.0f, 1.0f);
	rot.x = glm::clamp(glm::asin(a), -mmath::half_pi, mmath::half_pi) - mmath::half_pi;

	if (m.entry[0][0] <= 0.001f || m.entry[2][2] <= 0.001f) {
		auto ab = glm::atan(m.entry[0][2], m.entry[1][2]);
		rot.y = ab - mmath::half_pi;
	} else
		rot.y = glm::atan(m.entry[0][0], m.entry[1][0]);

	rot.z = 0.0f;
	return rot;
}

// Decompose a position to 3 basis vectors and the coefficients, given an euler rotation
void mmath::DecomposeToBasis(const glm::vec3& point, const glm::vec3& rotation,
	glm::vec3& forward, glm::vec3& right, glm::vec3& up, glm::vec3& coef) noexcept
{
	__m128 cosValues, sineValues;
	if constexpr (alignof(decltype(rotation)) == 16) {
		__m128 rot = _mm_load_ps(rotation.data.data);
		sineValues = _mm_sincos_ps(&cosValues, rot);
	} else {
		__m128 rot = _mm_loadu_ps(rotation.data.data);
		sineValues = _mm_sincos_ps(&cosValues, rot);
	}

	const auto cZsX = cosValues.m128_f32[2] * sineValues.m128_f32[0];
	const auto sXsZ = sineValues.m128_f32[0] * sineValues.m128_f32[2];

	forward = {
		cosValues.m128_f32[1] * cosValues.m128_f32[2],
		-cosValues.m128_f32[1] * sineValues.m128_f32[2],
		sineValues.m128_f32[1]
	};
	right = {
		cZsX * sineValues.m128_f32[1] + cosValues.m128_f32[0] * sineValues.m128_f32[2],
		cosValues.m128_f32[0] * cosValues.m128_f32[2] - sXsZ * sineValues.m128_f32[1],
		-cosValues.m128_f32[1] * sineValues.m128_f32[0]
	};
	up = {
		-cosValues.m128_f32[0] * cosValues.m128_f32[2] * sineValues.m128_f32[1] + sXsZ,
		cZsX + cosValues.m128_f32[0] * sineValues.m128_f32[1] * sineValues.m128_f32[2],
		cosValues.m128_f32[0] * cosValues.m128_f32[1]
	};

	coef = {
		glm::dot(point, forward),
		glm::dot(point, right),
		glm::dot(point, up)
	};
}

glm::mat4 mmath::Perspective(float fov, float aspect, const RE::NiFrustum& frustum) noexcept
{
	const auto range = frustum.fFar / (frustum.fNear - frustum.fFar);
	const auto height = 1.0f / glm::tan(fov * 0.5f);

	glm::mat4 proj;
	proj[0][0] = height;
	proj[0][1] = 0.0f;
	proj[0][2] = 0.0f;
	proj[0][3] = 0.0f;

	proj[1][0] = 0.0f;
	proj[1][1] = height * aspect;
	proj[1][2] = 0.0f;
	proj[1][3] = 0.0f;

	proj[2][0] = 0.0f;
	proj[2][1] = 0.0f;
	proj[2][2] = range * -1.0f;
	proj[2][3] = 1.0f;

	proj[3][0] = 0.0f;
	proj[3][1] = 0.0f;
	proj[3][2] = range * frustum.fNear;
	proj[3][3] = 0.0f;

	// exact match, save for 2,0 2,1 - looks like XMMatrixPerspectiveOffCenterLH with a slightly
	// different frustum or something. whatever, close enough.
	return proj;
}

glm::mat4 mmath::LookAt(const glm::vec3& pos, const glm::vec3& at) noexcept
{
	glm::vec3 up = {0.0f, 0.0f, 1.0f};

	const auto forward = glm::normalize(at - pos);
	const auto side = glm::normalize(glm::cross(up, forward));
	const auto u =  glm::normalize(glm::cross(forward, side));

	glm::mat4 result;
	result[0][0] = -side.x;
	result[1][0] = -side.y;
	result[2][0] = -side.z;
	result[3][0] = pos.x;

	result[0][1] = forward.x;
	result[1][1] = forward.y;
	result[2][1] = forward.z;
	result[3][1] = pos.y;

	result[0][2] = u.x;
	result[1][2] = u.y;
	result[2][2] = u.z;
	result[3][2] = pos.z;

	result[0][3] = 0.0f;
	result[1][3] = 0.0f;
	result[2][3] = 0.0f;
	result[3][3] = 1.0f;

	return result;
}

void mmath::Rotation::SetEuler(float pitch, float yaw) noexcept
{
	euler.x = pitch;
	euler.y = yaw;
	dirty = true;
}

void mmath::Rotation::SetQuaternion(const glm::quat& q) noexcept
{
	quat = q;
	euler.x = glm::pitch(q) * -1.0f;
	euler.y = glm::roll(q) * -1.0f;  // The game stores yaw in the Z axis
	dirty = true;
}

void mmath::Rotation::SetQuaternion(const RE::NiQuaternion& q) noexcept
{
	SetQuaternion(glm::quat{ q.w, q.x, q.y, q.z });
}

void mmath::Rotation::CopyFrom(const RE::TESObjectREFR* ref) noexcept
{
	SetEuler(ref->data.angle.x, ref->data.angle.z);
}

void mmath::Rotation::UpdateQuaternion() noexcept
{
	quat = glm::quat(glm::vec3{ -euler.x, 0.0f, -euler.y });
}

glm::quat mmath::Rotation::InverseQuat() const noexcept
{
	return glm::quat(glm::vec3{ euler.x, 0.0f, euler.y });
}

RE::NiQuaternion mmath::Rotation::InverseNiQuat() const noexcept
{
	const auto q = InverseQuat();
	return { q.w, q.x, q.y, q.z };
}

RE::NiQuaternion mmath::Rotation::ToNiQuat() const noexcept
{
	return { quat.w, quat.x, quat.y, quat.z };
}

RE::NiPoint2 mmath::Rotation::ToNiPoint2() const noexcept
{
	return { euler.x, euler.y };
}

RE::NiPoint3 mmath::Rotation::ToNiPoint3() const noexcept
{
	return { euler.x, 0.0f, euler.y };
}

glm::mat4 mmath::Rotation::ToRotationMatrix() noexcept
{
	if (dirty) {
		mat = glm::identity<glm::mat4>();
		mat = glm::rotate(mat, -euler.y, { 0.0f, 0.0f, 1.0f });  // yaw
		mat = glm::rotate(mat, -euler.x, { 1.0f, 0.0f, 0.0f });  // pitch
		dirty = false;
	}
	return mat;
}
