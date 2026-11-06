// This file is a part of Kazumi
// (https://github.com/Predidit/Kazumi).
//
// Copyright © 2024 Predidit
// All rights reserved.
// Use of this source code is governed by GPLv3 license that can be found in the
// LICENSE file.

#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <string>
#include <iostream>
#include <fstream>
#include <random>
#include <initializer_list>
#include "external_player_utils.h"

namespace {

bool IsExecutableFile(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring FindPotPlayer() {
    const wchar_t* executables[] = {L"PotPlayerMini64.exe", L"PotPlayerMini.exe"};
    // App Paths supports custom installations and per-user registrations.
    for (const auto executable : executables) {
        const std::wstring key =
            std::wstring(L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\") +
            executable;
        for (HKEY root : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
            for (DWORD view : {RRF_SUBKEY_WOW6464KEY, RRF_SUBKEY_WOW6432KEY}) {
                wchar_t path[32768] = {};
                DWORD size = sizeof(path);
                if (RegGetValueW(root, key.c_str(), nullptr,
                                 RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | view,
                                 nullptr, path, &size) == ERROR_SUCCESS) {
                    std::wstring candidate(path);
                    if (candidate.size() >= 2 && candidate.front() == L'"' &&
                        candidate.back() == L'"') {
                        candidate = candidate.substr(1, candidate.size() - 2);
                    }
                    if (IsExecutableFile(candidate)) return candidate;
                }
            }
        }
    }
    // Some installers do not register App Paths. Cover standard 32/64-bit
    // installation directories without executing anything found on PATH.
    for (const auto variable : {L"ProgramW6432", L"ProgramFiles", L"ProgramFiles(x86)",
                                L"LOCALAPPDATA"}) {
        wchar_t base[32768] = {};
        const DWORD length = GetEnvironmentVariableW(variable, base, 32768);
        if (length == 0 || length >= 32768) continue;
        for (const auto directory : {L"\\DAUM\\PotPlayer\\", L"\\PotPlayer\\",
                                     L"\\Programs\\PotPlayer\\"}) {
            for (const auto executable : executables) {
                const std::wstring candidate = std::wstring(base) + directory + executable;
                if (IsExecutableFile(candidate)) return candidate;
            }
        }
    }
    return L"";
}

}  // namespace

std::string ExternalPlayerUtils::OpenWithPotPlayer(const std::string& url) {
    // Defend the native boundary too: pass exactly one quoted URL argument,
    // never a shell command or a playlist containing untrusted directives.
    if ((url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0) ||
        url.find_first_of("\"\\") != std::string::npos) {
        return "unsupportedSource";
    }
    for (unsigned char character : url) {
        if (character <= 0x20 || character == 0x7f) return "unsupportedSource";
    }
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                           url.c_str(), -1, nullptr, 0);
    if (length == 0) return "unsupportedSource";
    std::wstring wide_url(static_cast<size_t>(length), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, url.c_str(), -1,
                             wide_url.data(), length)) {
        return "unsupportedSource";
    }
    wide_url.resize(length - 1);
    const std::wstring executable = FindPotPlayer();
    if (executable.empty()) return "notInstalled";
    const std::wstring arguments = L"\"" + wide_url + L"\"";
    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    info.lpVerb = L"open";
    info.lpFile = executable.c_str();
    info.lpParameters = arguments.c_str();
    info.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&info)) return "failed";
    if (info.hProcess) {
        DWORD exit_code = 0;
        const bool exited = GetExitCodeProcess(info.hProcess, &exit_code) &&
                            exit_code != STILL_ACTIVE && exit_code != 0;
        CloseHandle(info.hProcess);
        if (exited) return "failed";
    }
    // A successful OS handoff does not guarantee remote codec/source support.
    return "launched";
}

void ExternalPlayerUtils::OpenWithPlayer(const char* url) {
    // temp file path
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);

    // Generate a random file name
    std::wstring randomFileName = L"kazumi_stream_";
    std::random_device rd;
    std::mt19937 eng(rd());
    std::uniform_int_distribution<> distr(10000000, 99999999);

    randomFileName += std::to_wstring(distr(eng)) + L".m3u8";

    wchar_t tempFile[MAX_PATH];
    wcscpy_s(tempFile, tempPath);
    wcscat_s(tempFile, randomFileName.c_str());

    // write the URL to the temp file
    std::wofstream outFile(tempFile);
    if (outFile.is_open()) {
        outFile << L"#EXTM3U\n";
        outFile << std::wstring(url, url + strlen(url));
        outFile.close();
    } else {
        return;
    }

    SHELLEXECUTEINFO execInfo = {0};
    execInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    execInfo.fMask = SEE_MASK_INVOKEIDLIST;
    execInfo.lpVerb = L"openas";
    execInfo.lpFile = tempFile;
    execInfo.nShow = SW_SHOWNORMAL;

    ShellExecuteEx(&execInfo);

    // DeleteFileW(tempFile);
}
