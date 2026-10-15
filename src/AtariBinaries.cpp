#include "PlatformTypes.h"
#include "AtariBinaries.h"
#include <filesystem>
#include "Global.h"


#include <map>


static std::map<TrackerDriverVersion, CByteArray*> m_trackerDriverVersionBinary;

// TODO Move to CResourceUtility class

CByteArray* LoadByteArray(const CString& filePath)
{
    CFileStatus status;
    if (CFile::GetStatus(filePath, status) == 0) {
        return nullptr;
    }
    if ((size_t)status.m_size > CAtari::MEMORY_SIZE) {
        return nullptr;
    }
    auto byteArray = new CByteArray();
    byteArray->SetSize(status.m_size);
    CFile file(filePath, CFile::modeRead);
    auto sizeRead = file.Read(byteArray->GetData(), (UINT)byteArray->GetSize());
    if (sizeRead != (UINT)status.m_size) {
        delete byteArray;
        return nullptr;
    }
    return byteArray;
}

CByteArray* LoadResourceByteArray(const std::filesystem::path& relativePath, const CString& fileName)
{
    CString filePath = GetResourceFilePath(relativePath, fileName);
    return LoadByteArray(filePath);
}


CString CRmtAtariBinaries::GetTrackerDriverFilePath(TrackerDriverVersion trackerDriverVersion)
{
    CString fileName;
    fileName.Format("rmt_driver_v%d.obx", (int)trackerDriverVersion);
    // assembled with the separator operator: the path is shown to the user, and a literal forward slash would survive
    // into the middle of an otherwise backslashed Windows path
    return GetResourceFilePath(std::filesystem::path("resources") / "drivers", fileName);
}

CString CRmtAtariBinaries::GetMissingTrackerDriverMessage(TrackerDriverVersion trackerDriverVersion)
{
    CString message;
    message.Format("Could not load the player routines of the selected tracker driver:\n'%s'\n\n"
                   "Until that file is back, RITMO plays and exports with the driver loaded before, or in silence if there "
                   "is none.\n",
                   (LPCTSTR)GetTrackerDriverFilePath(trackerDriverVersion));
    return message;
}

bool CRmtAtariBinaries::GetTrackerDriverBinary(TrackerDriverVersion trackerDriverVersion, byte*& binary, WORD& size)
{
    binary = nullptr;
    size = 0;

    auto byteArrayIt = m_trackerDriverVersionBinary.find(trackerDriverVersion);
    CByteArray* byteArray;
    if (byteArrayIt != m_trackerDriverVersionBinary.end()) {
        byteArray = (*byteArrayIt).second;
    } else {
        byteArray = LoadByteArray(GetTrackerDriverFilePath(trackerDriverVersion));
        if (!byteArray) {
            return false;
        }
        m_trackerDriverVersionBinary.insert({ trackerDriverVersion, byteArray });
    }

    binary = byteArray->GetData();
    size = (WORD)byteArray->GetSize(); // TODO: Return *CByteArray
    return true;
}

CByteArray* CRmtAtariBinaries::LoadPlayerBinary(const CString& fileName)
{
    return LoadResourceByteArray(std::filesystem::path("resources/players"), fileName);
}

bool CRmtAtariBinaries::GetVUPlayerBinary(byte*& binary, WORD& size)
{
    binary = nullptr;
    size = 0;

    auto byteArray = LoadResourceByteArray(std::filesystem::path("resources/players"), "vu_player_v2.obx");
    if (!byteArray) {
        return false;
    }
    binary = byteArray->GetData();
    size = (WORD)byteArray->GetSize(); // TODO: Return *CByteArray
    return true;
}
