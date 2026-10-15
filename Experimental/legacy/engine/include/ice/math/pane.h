#pragma once
namespace ice::math {

class plane {
public:
	plane(void);
	plane(const vec3 &normal, const ice::math::real offset);

	const vec3     &GetNormal(void) const;
	ice::math::real GetOffset(void) const;

	bool operator==(const plane &plane) const;
	bool operator!=(const plane &plane) const;

private:
	vec3            m_v3Norm;
	ice::math::real m_Offset;
};

} // namespace ice::math
