// stdafx.h : ��׼ϵͳ�����ļ��İ����ļ���
// ���Ǿ���ʹ�õ��������ĵ�
// �ض�����Ŀ�İ����ļ�
//

#pragma once

#include "targetver.h"

// C/C++ standard headers
#include <cstdint>
#include <array>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <sstream>
#include <iomanip>

// Other external headers
// Windows headers
#include <tchar.h>
#include <Windows.h>
#include <strsafe.h>
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

// Original headers
#include "MyVersionHelpers.h"
#include "../DriverUtil6/scope_exit.h"
#include "../DriverUtil6/unique_resource.h"

#pragma warning(disable : 4091)
// TODO:  �ڴ˴����ó�����Ҫ������ͷ�ļ�
