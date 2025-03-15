#include "BetterTelekinesis/Util.h"

namespace Util
{
	void report_and_fail_timed(const std::string& a_message)
	{
		logger::error(fmt::runtime(a_message));
		MessageBoxTimeoutA(nullptr, a_message.c_str(), a_message.c_str(), MB_SYSTEMMODAL, 0, 7000);
		TerminateProcess(GetCurrentProcess(), 1);
	}

	RE::NiPoint3 Translate(const RE::NiTransform& transform, const RE::NiPoint3 amount)
	{
		RE::NiPoint3 result = RE::NiPoint3();

		auto a = glm::vec3{ amount.x, amount.y, amount.z };
		auto m = &transform.rotate.entry[0][0];

		result.x = m[0] * a[0] + m[1] * a[1] + m[2] * a[2] + transform.translate.x;
		result.y = m[3] * a[0] + m[4] * a[1] + m[5] * a[2] + transform.translate.y;
		result.z = m[6] * a[0] + m[7] * a[1] + m[8] * a[2] + transform.translate.z;

		return result;
	}

	RE::NiPoint3 GetEulerAngles(const RE::NiMatrix3& rotate)
	{
		auto result = RE::NiPoint3();
		if (std::abs(rotate.entry[0][2] + 1.0f) < std::numeric_limits<float>::denorm_min()) {
			result.x = 0.0f;
			result.y = RE::NI_HALF_PI;
			result.z = RE::NiFastATan2(rotate.entry[1][0], rotate.entry[2][0]);
			return result;
		}

		if (std::abs(rotate.entry[0][2] - 1.0f) < std::numeric_limits<float>::denorm_min()) {
			result.x = 0.0f;
			result.y = -RE::NI_HALF_PI;
			result.z = RE::NiFastATan2(-rotate.entry[1][0], -rotate.entry[2][0]);
			return result;
		}

		float x1 = -std::asin(rotate.entry[0][2]);
		float cx1 = std::cos(x1);
		result.x = x1;
		result.y = RE::NiFastATan2(rotate.entry[1][2] / cx1, rotate.entry[2][2] / cx1);
		result.z = RE::NiFastATan2(rotate.entry[0][1] / cx1, rotate.entry[0][0] / cx1);

		return result;
	}

	RE::NiPoint3 GetEulerAngles2(const RE::NiMatrix3& rotate)
	{
		auto result = RE::NiPoint3();
		if (rotate.entry[0][2] < 1.0f) {
			if (rotate.entry[0][2] > -1.0f) {
				result.y = std::asin(rotate.entry[0][2]);
				result.x = RE::NiFastATan2(-rotate.entry[1][2], rotate.entry[2][2]);
				result.z = RE::NiFastATan2(-rotate.entry[0][1], rotate.entry[0][0]);
			} else {
				result.y = -RE::NI_HALF_PI;
				result.x = -RE::NiFastATan2(rotate.entry[1][0], rotate.entry[1][1]);
				result.z = 0.0f;
			}
		} else {
			result.y = RE::NI_HALF_PI;
			result.x = RE::NiFastATan2(rotate.entry[1][0], rotate.entry[1][1]);
			result.z = 0.0f;
		}

		return result;
	}

	glm::mat3 NiMatrix3ToGlm(const RE::NiMatrix3& m)
	{
		glm::mat3 result = glm::mat3();
		result[0][0] = m.entry[0][0];
		result[0][1] = m.entry[0][1];
		result[0][2] = m.entry[0][2];
		result[1][0] = m.entry[1][0];
		result[1][1] = m.entry[1][1];
		result[1][2] = m.entry[1][2];
		result[2][0] = m.entry[2][0];
		result[2][1] = m.entry[2][1];
		result[2][2] = m.entry[2][2];

		return result;
	}

	RE::NiMatrix3 GlmToNiMatrix3(const glm::mat3& m)
	{
		RE::NiMatrix3 result = RE::NiMatrix3();
		result.entry[0][0] = m[0][0];
		result.entry[0][1] = m[0][1];
		result.entry[0][2] = m[0][2];
		result.entry[1][0] = m[1][0];
		result.entry[1][1] = m[1][1];
		result.entry[1][2] = m[1][2];
		result.entry[2][0] = m[2][0];
		result.entry[2][1] = m[2][1];
		result.entry[2][2] = m[2][2];

		return result;
	}

	glm::mat4 NiTransformToGlm(const RE::NiTransform& t)
	{
		auto matrix = t.rotate;
		glm::mat4 result = glm::mat4();
		result[0][0] = matrix.entry[0][0];
		result[0][1] = matrix.entry[0][1];
		result[0][2] = matrix.entry[0][2];
		result[0][3] = 0.0f;
		result[1][0] = matrix.entry[1][0];
		result[1][1] = matrix.entry[1][1];
		result[1][2] = matrix.entry[1][2];
		result[1][3] = 0.0f;
		result[2][0] = matrix.entry[2][0];
		result[2][1] = matrix.entry[2][1];
		result[2][2] = matrix.entry[2][2];
		result[2][3] = 0.0f;
		result[3][0] = t.translate.x;
		result[3][1] = t.translate.y;
		result[3][2] = t.translate.z;
		result[3][3] = 1.0f;

		return result;
	}

	glm::mat3 ViewMatrixToRotationMatrix(const glm::mat4& matrix)
	{
		glm::mat3 result = glm::mat3();
		result[0][0] = matrix[0][0];
		result[0][1] = matrix[0][1];
		result[0][2] = matrix[0][2];
		result[1][0] = matrix[1][0];
		result[1][1] = matrix[1][1];
		result[1][2] = matrix[1][2];
		result[2][0] = matrix[2][0];
		result[2][1] = matrix[2][1];
		result[2][2] = matrix[2][2];

		return result;
	}

	RE::NiPoint3 viewMatrixToTranslate(const glm::mat4& matrix)
	{
		RE::NiPoint3 result = RE::NiPoint3();
		result.x = matrix[3][0];
		result.y = matrix[3][1];
		result.z = matrix[3][2];
		return result;
	}

	CachedFormList::CachedFormList() = default;

	CachedFormList* CachedFormList::TryParse(const std::string& input, std::string settingNameForLog, bool warnOnMissingForm, bool dontWriteAnythingToLog)
	{
		if (settingNameForLog.empty()) {
			settingNameForLog = "unknown form list setting";
		}

		auto ls = new CachedFormList();
		char Char = ';';
		auto spl = StringHelpers::split(input, Char, true);
		for (auto& x : spl) {
			std::string idstr;
			std::string fileName;

			auto ix = x.find(L':');
			if (ix <= 0) {
				if (!dontWriteAnythingToLog) {
					logger::warn(fmt::runtime("Failed to parse form for " + settingNameForLog + "! Invalid input: `" + x + "`."));
				}

				delete ls;
				return nullptr;
			}

			idstr = x.substr(0, ix);
			fileName = x.substr(ix + 1);

			if (!std::ranges::all_of(idstr.begin(), idstr.end(), [](wchar_t q) { return (q >= L'0' && q <= L'9') || (q >= L'a' && q <= L'f') || (q >= L'A' && q <= L'F'); })) {
				if (!dontWriteAnythingToLog) {
					logger::warn(fmt::runtime("Failed to parse form for " + settingNameForLog + "! Invalid form ID: `" + idstr + "`."));
				}

				delete ls;
				return nullptr;
			}

			if (fileName.empty()) {
				if (!dontWriteAnythingToLog) {
					logger::warn(fmt::runtime("Failed to parse form for " + settingNameForLog + "! Missing file name."));
				}

				delete ls;
				return nullptr;
			}

			RE::FormID id = 0;
			bool sucess;
			try {
				id = stoi(idstr, nullptr, 16);
				sucess = true;
			} catch (std::exception&) {
				sucess = false;
			}
			if (!sucess) {
				if (!dontWriteAnythingToLog) {
					logger::warn(fmt::runtime("Failed to parse form for " + settingNameForLog + "! Invalid form ID: `" + idstr + "`."));
				}

				delete ls;
				return nullptr;
			}

			id &= 0x00FFFFFF;
			if (auto file = RE::TESDataHandler::GetSingleton()->LookupLoadedModByName(fileName); file) {
				if (!RE::TESDataHandler::GetSingleton()->LookupFormID(id, fileName)) {
					if (!dontWriteAnythingToLog && warnOnMissingForm) {
						logger::warn(fmt::runtime("Failed to find form for " + settingNameForLog + "! Form ID was 0x{:x} and file was " + fileName + "."), id);
					}
					continue;
				}
			}
			auto form = RE::TESDataHandler::GetSingleton()->LookupForm(id, fileName);
			auto formID = RE::TESDataHandler::GetSingleton()->LookupFormID(id, fileName);
			if (!form || !formID) {
				if (!dontWriteAnythingToLog) {
					logger::warn(fmt::runtime("Invalid form detected while adding form to " + settingNameForLog + "! Possible invalid form ID: `0x{:x}`."), formID);
				}
				continue;
			}
			if (ls->Ids.insert(formID).second) {
				logger::info(fmt::runtime("Form 0x{:x} was successfully added to " + settingNameForLog), formID);
				ls->Forms.push_back(form);
			}
		}

		return ls;
	}

	bool CachedFormList::Contains(RE::TESForm* form)
	{
		if (form == nullptr)
			return false;

		return Contains(form->formID);
	}

	bool CachedFormList::Contains(unsigned int formId)
	{
		return std::ranges::find(this->Ids.begin(), this->Ids.end(), formId) != this->Ids.end();
	}

	std::vector<RE::TESForm*> CachedFormList::getAll() const
	{
		return this->Forms;
	}
}
