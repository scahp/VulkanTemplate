#pragma once

struct jSimpleVec2
{
	float x;
	float y;

	bool operator == (const jSimpleVec2& other) const
	{
		return ((x == other.x) && (y == other.y));
	}
};

struct jSimpleVec3
{
	float x;
	float y;
	float z;
	
	bool operator == (const jSimpleVec3& other) const
	{
		return ((x == other.x) && (y == other.y) && (z == other.z));
	}
};
