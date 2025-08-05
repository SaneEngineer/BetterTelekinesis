// Adapted from https://github.com/mwilsnd/SkyrimSE-SmoothCam/blob/master/SmoothCam/source/raycast.cpp
#include "BetterTelekinesis/RaycastHelper.h"

Raycast::RayCollector::RayCollector() = default;

void Raycast::RayCollector::AddRayHit(const RE::hkpCdBody& body, const RE::hkpShapeRayCastCollectorOutput& hitInfo)
{
	HitResult hit{};
	hit.hitFraction = hitInfo.hitFraction;
	hit.normal = {
		hitInfo.normal.quad.m128_f32[0],
		hitInfo.normal.quad.m128_f32[1],
		hitInfo.normal.quad.m128_f32[2]
	};

	const RE::hkpCdBody* obj = &body;
	while (obj) {
		if (!obj->parent)
			break;
		obj = obj->parent;
	}

	hit.body = obj;
	if (!hit.body)
		return;

	const auto collisionObj = static_cast<const RE::hkpCollidable*>(hit.body);
	const auto flags = collisionObj->broadPhaseHandle.collisionFilterInfo;

	const uint64_t m = 1ULL << flags.filter;
	constexpr uint64_t filter = 0x40122716;  //@TODO
	if ((m & filter) != 0) {
		if (!objectFilter.empty()) {
			for (const auto filteredObj : objectFilter) {
				if (hit.getAVObject() == filteredObj)
					return;
			}
		}

		earlyOutHitFraction = hit.hitFraction;
		hits.push_back(hit);
	}
}

const std::vector<Raycast::RayCollector::HitResult>& Raycast::RayCollector::GetHits()
{
	return hits;
}

void Raycast::RayCollector::Reset()
{
	earlyOutHitFraction = 1.0f;
	hits.clear();
	objectFilter.clear();
}

RE::NiAVObject* Raycast::RayCollector::HitResult::getAVObject()
{
	typedef RE::NiAVObject* (*_GetUserData)(const RE::hkpCdBody*);
	static auto getAVObject = REL::Relocation<_GetUserData>(RELOCATION_ID(76160, 77988));
	return body ? getAVObject(body) : nullptr;
}

RE::NiAVObject* Raycast::getAVObject(const RE::hkpCdBody* body)
{
	typedef RE::NiAVObject* (*_GetUserData)(const RE::hkpCdBody*);
	static auto getAVObject = REL::Relocation<_GetUserData>(RELOCATION_ID(76160, 77988));
	return body ? getAVObject(body) : nullptr;
}

Raycast::RayResult Raycast::hkpCastRay(const glm::vec4& start, const glm::vec4& end) noexcept
{
#ifndef NDEBUG
	if (!mmath::IsValid(start) || !mmath::IsValid(end)) {
		__debugbreak();
		return {};
	}
#endif
	constexpr auto hkpScale = 0.0142875f;
	const glm::vec4 dif = end - start;

	constexpr auto one = 1.0f;
	const auto from = start * hkpScale;
	const auto to = dif * hkpScale;

	RE::hkpWorldRayCastInput pickRayInput{};
	pickRayInput.from = RE::hkVector4(from.x, from.y, from.z, one);
	pickRayInput.to = RE::hkVector4(0.0, 0.0, 0.0, 0.0);

	auto collector = RayCollector();
	collector.Reset();

	RE::bhkPickData pickData{};
	pickData.rayInput = pickRayInput;
	pickData.ray = RE::hkVector4(to.x, to.y, to.z, one);
	pickData.rayHitCollectorA8 = reinterpret_cast<RE::hkpClosestRayHitCollector*>(&collector);

	const auto ply = RE::PlayerCharacter::GetSingleton();
	auto cell = ply->GetParentCell();
	if (!cell)
		return {};

	if (ply->loadedData && ply->loadedData->data3D)
		collector.AddFilter(ply->loadedData->data3D.get());

	RayCollector::HitResult best{};
	best.hitFraction = 1.0f;
	glm::vec4 bestPos = {};

	RayResult result;

	try {
		auto physicsWorld = cell->GetbhkWorld();
		if (physicsWorld) {
			physicsWorld->PickObject(pickData);
		}
	} catch (...) {
	}

	for (auto& hit : collector.GetHits()) {
		const auto pos = (dif * hit.hitFraction) + start;
		if (best.body == nullptr) {
			best = hit;
			bestPos = pos;
			continue;
		}

		if (hit.hitFraction < best.hitFraction) {
			best = hit;
			bestPos = pos;
		}
	}

	result.hitArray = collector.GetHits();

	//result.hitPos = bestPos;
	result.rayLength = length(bestPos - start);

	if (!best.body)
		return result;
	auto av = best.getAVObject();
	result.hit = av != nullptr;

	if (result.hit) {
		result.hitObject = av;
	}

	return result;
}

namespace BetterTelekinesis
{
	void RaycastHelper::do_split_raycast(glm::vec4& headPos, glm::vec4& camPos, glm::vec4& endPos, RE::TESObjectCELL* cell, std::vector<RE::NiAVObject*>& plrNodes)
	{
		auto ray = Raycast::hkpCastRay(camPos, endPos);

		float frac = 1.0f;

		for (auto& [normal, hitFraction, body] : ray.hitArray) {
			if (hitFraction >= frac || body == nullptr) {
				continue;
			}

			const auto collisionObj = static_cast<const RE::hkpCollidable*>(body);
			const auto flags = collisionObj->GetCollisionLayer();
			unsigned long long mask = static_cast<unsigned long long>(1) << static_cast<int>(flags);
			if ((RaycastMask & mask) == 0) {
				continue;
			}

			if (ray.hitObject != nullptr) {
				bool ispl = false;
				for (auto pi : plrNodes) {
					if (pi != nullptr && IsRaycastHitNodeTest(ray, pi->AsNode())) {
						ispl = true;
						break;
					}
				}

				if (ispl) {
					continue;
				}
			}

			frac = hitFraction;
		}

		if (frac < 1.0f) {
			for (int i = 0; i < 3; i++) {
				if (i == 2) {
					endPos[i] = (endPos[i] - camPos[i]) * std::max(frac - 0.01f, 0.0f) + camPos[i];
				} else {
					endPos[i] = (endPos[i] - camPos[i]) * frac + camPos[i];
				}
			}
		}

		if (length(headPos) == 0.0) {
			return;
		}

		ray = Raycast::hkpCastRay(headPos, endPos);

		frac = 1.0f;

		for (auto& [normal, hitFraction, body] : ray.hitArray) {
			if (hitFraction >= frac || body == nullptr) {
				continue;
			}

			const auto collisionObj = static_cast<const RE::hkpCollidable*>(body);
			const auto flags = collisionObj->GetCollisionLayer();
			unsigned long long mask = static_cast<unsigned long long>(1) << static_cast<int>(flags);
			if ((RaycastMask & mask) == 0) {
				continue;
			}

			auto obj = ray.hitObject;
			if (obj != nullptr) {
				bool ispl = false;
				for (auto pi : plrNodes) {
					if (pi != nullptr && IsRaycastHitNodeTest(ray, pi->AsNode())) {
						ispl = true;
						break;
					}
				}

				if (ispl) {
					continue;
				}
			}

			frac = hitFraction;
		}

		if (frac < 1.0f) {
			for (int i = 0; i < 3; i++) {
				if (i == 2) {
					endPos[i] = (endPos[i] - headPos[i]) * std::max(frac - 0.01f, 0.0f) + headPos[i];
				} else {
					endPos[i] = (endPos[i] - headPos[i]) * frac + headPos[i];
				}
			}
		}
	}

	RE::TESForm* RaycastHelper::GetRaycastHitBaseForm(const Raycast::RayResult& r) const
	{
		RE::TESForm* result = nullptr;
		try {
			auto o = r.hitObject;
			int tries = 0;
			while (o != nullptr && tries++ < 10) {
				const auto userdata = o->GetUserData();
				if (userdata != nullptr) {
					auto baseForm = userdata->GetOwner();
					if (baseForm != nullptr) {
						{
							result = baseForm;
							return result;
						}
						break;
					}
				}
				o = o->parent;
			}
		} catch (...) {
		}
		return result;
	}

	bool RaycastHelper::IsRaycastHitTest(const Raycast::RayResult& r, const std::function<bool(RE::FormID)>& func)
	{
		bool result = false;
		try {
			auto o = r.hitObject;
			int tries = 0;
			while (o != nullptr && tries++ < 10) {
				const auto userdata = o->GetUserData();
				if (userdata != nullptr) {
					auto baseForm = userdata->GetOwner();
					if (baseForm != nullptr) {
						{
							if (func(baseForm->formID))
								result = true;
						}
						break;
					}
				}
				o = o->parent;
			}
		} catch (...) {
		}
		return result;
	}

	bool RaycastHelper::IsRaycastHitNodeTest(const Raycast::RayResult& r, RE::NiNode* node)
	{
		bool result = false;
		try {
			auto o = r.hitObject;
			int tries = 0;
			while (o != nullptr && tries++ < 10) {
				const auto userdata = o->GetUserData();
				const auto nodeUserData = node->GetUserData();
				if (userdata != nullptr && nodeUserData != nullptr) {
					auto baseForm = userdata->GetOwner();
					auto nodeBaseForm = nodeUserData->GetOwner();
					if (baseForm != nullptr && nodeBaseForm != nullptr) {
						{
							if (baseForm->formID == nodeBaseForm->formID) {
								result = true;
							}
						}
						break;
					}
				}
				o = o->parent;
			}
		} catch (...) {
		}
		return result;
	}
}
