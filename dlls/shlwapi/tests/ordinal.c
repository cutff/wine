/* Unit test suite for SHLWAPI ordinal functions
 *
 * Copyright 2004 Jon Griffiths
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdio.h>

#define COBJMACROS
#include "wine/test.h"
#include "winbase.h"
#include "winerror.h"
#include "winuser.h"
#include "ole2.h"
#include "oaidl.h"
#include "ocidl.h"
#include "mlang.h"
#include "shlwapi.h"

/* Function ptrs for ordinal calls */
static HMODULE hShlwapi;
static int (WINAPI *pSHSearchMapInt)(const int*,const int*,int,int);
static HRESULT (WINAPI *pGetAcceptLanguagesA)(LPSTR,LPDWORD);

static HANDLE (WINAPI *pSHAllocShared)(LPCVOID,DWORD,DWORD);
static LPVOID (WINAPI *pSHLockShared)(HANDLE,DWORD);
static BOOL   (WINAPI *pSHUnlockShared)(LPVOID);
static BOOL   (WINAPI *pSHFreeShared)(HANDLE,DWORD);
static HRESULT(WINAPIV *pSHPackDispParams)(DISPPARAMS*,VARIANTARG*,UINT,...);
static HRESULT(WINAPI *pIConnectionPoint_SimpleInvoke)(IConnectionPoint*,DISPID,DISPPARAMS*);
static HRESULT(WINAPI *pIConnectionPoint_InvokeWithCancel)(IConnectionPoint*,DISPID,DISPPARAMS*,DWORD,DWORD);
static HRESULT(WINAPI *pConnectToConnectionPoint)(IUnknown*,REFIID,BOOL,IUnknown*, LPDWORD,IConnectionPoint **);
static HRESULT(WINAPI *pSHPropertyBag_ReadLONG)(IPropertyBag *,LPCWSTR,LPLONG);
static LONG   (WINAPI *pSHSetWindowBits)(HWND, INT, UINT, UINT);
static INT    (WINAPI *pSHFormatDateTimeA)(const FILETIME UNALIGNED*, DWORD*, LPSTR, UINT);
static INT    (WINAPI *pSHFormatDateTimeW)(const FILETIME UNALIGNED*, DWORD*, LPWSTR, UINT);
static DWORD  (WINAPI *pSHGetObjectCompatFlags)(IUnknown*, const CLSID*);
static BOOL   (WINAPI *pGUIDFromStringA)(LPSTR, CLSID *);

static HMODULE hmlang;
static HRESULT (WINAPI *pLcidToRfc1766A)(LCID, LPSTR, INT);

static const CHAR ie_international[] = {
    'S','o','f','t','w','a','r','e','\\',
    'M','i','c','r','o','s','o','f','t','\\',
    'I','n','t','e','r','n','e','t',' ','E','x','p','l','o','r','e','r','\\',
    'I','n','t','e','r','n','a','t','i','o','n','a','l',0};
static const CHAR acceptlanguage[] = {
    'A','c','c','e','p','t','L','a','n','g','u','a','g','e',0};


static void test_GetAcceptLanguagesA(void)
{
    static LPCSTR table[] = {"de,en-gb;q=0.7,en;q=0.3",
                             "de,en;q=0.3,en-gb;q=0.7", /* sorting is ignored */
                             "winetest",    /* content is ignored */
                             "de-de,de;q=0.5",
                             "de",
                             NULL};

    DWORD exactsize;
    char original[512];
    char language[32];
    char buffer[64];
    HKEY hroot = NULL;
    LONG res_query = ERROR_SUCCESS;
    LONG lres;
    HRESULT hr;
    DWORD maxlen = sizeof(buffer) - 2;
    DWORD len;
    LCID lcid;
    LPCSTR entry;
    INT i = 0;

    if (!pGetAcceptLanguagesA) {
        win_skip("GetAcceptLanguagesA is not available\n");
        return;
    }

    lcid = GetUserDefaultLCID();

    /* Get the original Value */
    lres = RegOpenKeyA(HKEY_CURRENT_USER, ie_international, &hroot);
    if (lres) {
        skip("RegOpenKey(%s) failed: %d\n", ie_international, lres);
        return;
    }
    len = sizeof(original);
    original[0] = 0;
    res_query = RegQueryValueExA(hroot, acceptlanguage, 0, NULL, (PBYTE)original, &len);

    RegDeleteValue(hroot, acceptlanguage);

    /* Some windows versions use "lang-COUNTRY" as default */
    memset(language, 0, sizeof(language));
    len = GetLocaleInfoA(lcid, LOCALE_SISO639LANGNAME, language, sizeof(language));

    if (len) {
        lstrcat(language, "-");
        memset(buffer, 0, sizeof(buffer));
        len = GetLocaleInfoA(lcid, LOCALE_SISO3166CTRYNAME, buffer, sizeof(buffer) - len - 1);
        lstrcat(language, buffer);
    }
    else
    {
        /* LOCALE_SNAME has additional parts in some languages. Try only as last chance */
        memset(language, 0, sizeof(language));
        len = GetLocaleInfoA(lcid, LOCALE_SNAME, language, sizeof(language));
    }

    /* get the default value */
    len = maxlen;
    memset(buffer, '#', maxlen);
    buffer[maxlen] = 0;
    hr = pGetAcceptLanguagesA( buffer, &len);

    if (hr != S_OK) {
        win_skip("GetAcceptLanguagesA failed with 0x%x\n", hr);
        goto restore_original;
    }

    if (lstrcmpA(buffer, language)) {
        /* some windows versions use "lang" or "lang-country" as default */
        language[0] = 0;
        if (pLcidToRfc1766A) {
            hr = pLcidToRfc1766A(lcid, language, sizeof(language));
            ok(hr == S_OK, "LcidToRfc1766A returned 0x%x and %s\n", hr, language);
        }
    }

    ok(!lstrcmpA(buffer, language),
        "have '%s' (searching for '%s')\n", language, buffer);

    if (lstrcmpA(buffer, language)) {
        win_skip("no more ideas, how to build the default language '%s'\n", buffer);
        goto restore_original;
    }

    trace("detected default: %s\n", language);
    while ((entry = table[i])) {

        exactsize = lstrlenA(entry);

        lres = RegSetValueExA(hroot, acceptlanguage, 0, REG_SZ, (const BYTE *) entry, exactsize + 1);
        ok(!lres, "got %d for RegSetValueExA: %s\n", lres, entry);

        /* len includes space for the terminating 0 before vista/w2k8 */
        len = exactsize + 2;
        memset(buffer, '#', maxlen);
        buffer[maxlen] = 0;
        hr = pGetAcceptLanguagesA( buffer, &len);
        ok(((hr == E_INVALIDARG) && (len == 0)) ||
            (SUCCEEDED(hr) &&
            ((len == exactsize) || (len == exactsize+1)) &&
            !lstrcmpA(buffer, entry)),
            "+2_#%d: got 0x%x with %d and %s\n", i, hr, len, buffer);

        len = exactsize + 1;
        memset(buffer, '#', maxlen);
        buffer[maxlen] = 0;
        hr = pGetAcceptLanguagesA( buffer, &len);
        ok(((hr == E_INVALIDARG) && (len == 0)) ||
            (SUCCEEDED(hr) &&
            ((len == exactsize) || (len == exactsize+1)) &&
            !lstrcmpA(buffer, entry)),
            "+1_#%d: got 0x%x with %d and %s\n", i, hr, len, buffer);

        len = exactsize;
        memset(buffer, '#', maxlen);
        buffer[maxlen] = 0;
        hr = pGetAcceptLanguagesA( buffer, &len);

        /* There is no space for the string in the registry.
           When the buffer is large enough, the default language is returned

           When the buffer is to small for that fallback, win7_32 and w2k8_64
           and above fail with HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), but
           recent os succeed and return a partial result while
           older os succeed and overflow the buffer */

        ok(((hr == E_INVALIDARG) && (len == 0)) ||
            (((hr == S_OK) && !lstrcmpA(buffer, language)  && (len == lstrlenA(language))) ||
            ((hr == S_OK) && !memcmp(buffer, language, len)) ||
            ((hr == __HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) && !len)),
            "==_#%d: got 0x%x with %d and %s\n", i, hr, len, buffer);

        if (exactsize > 1) {
            len = exactsize - 1;
            memset(buffer, '#', maxlen);
            buffer[maxlen] = 0;
            hr = pGetAcceptLanguagesA( buffer, &len);
            ok(((hr == E_INVALIDARG) && (len == 0)) ||
                (((hr == S_OK) && !lstrcmpA(buffer, language)  && (len == lstrlenA(language))) ||
                ((hr == S_OK) && !memcmp(buffer, language, len)) ||
                ((hr == __HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) && !len)),
                "-1_#%d: got 0x%x with %d and %s\n", i, hr, len, buffer);
        }

        len = 1;
        memset(buffer, '#', maxlen);
        buffer[maxlen] = 0;
        hr = pGetAcceptLanguagesA( buffer, &len);
        ok(((hr == E_INVALIDARG) && (len == 0)) ||
            (((hr == S_OK) && !lstrcmpA(buffer, language)  && (len == lstrlenA(language))) ||
            ((hr == S_OK) && !memcmp(buffer, language, len)) ||
            ((hr == __HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) && !len)),
            "=1_#%d: got 0x%x with %d and %s\n", i, hr, len, buffer);

        len = maxlen;
        hr = pGetAcceptLanguagesA( NULL, &len);

        /* w2k3 and below: E_FAIL and untouched len,
           since w2k8: S_OK and needed size (excluding 0) */
        ok( ((hr == S_OK) && (len == exactsize)) ||
            ((hr == E_FAIL) && (len == maxlen)),
            "NULL,max #%d: got 0x%x with %d and %s\n", i, hr, len, buffer);

        i++;
    }

    /* without a value in the registry, a default language is returned */
    RegDeleteValue(hroot, acceptlanguage);

    len = maxlen;
    memset(buffer, '#', maxlen);
    buffer[maxlen] = 0;
    hr = pGetAcceptLanguagesA( buffer, &len);
    ok( ((hr == S_OK) && (len == lstrlenA(language))),
        "max: got 0x%x with %d and %s (expected S_OK with %d and '%s'\n",
        hr, len, buffer, lstrlenA(language), language);

    len = 2;
    memset(buffer, '#', maxlen);
    buffer[maxlen] = 0;
    hr = pGetAcceptLanguagesA( buffer, &len);
    ok( (((hr == S_OK) || (hr == E_INVALIDARG)) && !memcmp(buffer, language, len)) ||
        ((hr == __HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) && !len),
        "=2: got 0x%x with %d and %s\n", hr, len, buffer);

    len = 1;
    memset(buffer, '#', maxlen);
    buffer[maxlen] = 0;
    hr = pGetAcceptLanguagesA( buffer, &len);
    /* When the buffer is to small, win7_32 and w2k8_64 and above fail with
       HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), other versions suceed
       and return a partial 0 terminated result while other versions
       fail with E_INVALIDARG and return a partial unterminated result */
    ok( (((hr == S_OK) || (hr == E_INVALIDARG)) && !memcmp(buffer, language, len)) ||
        ((hr == __HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) && !len),
        "=1: got 0x%x with %d and %s\n", hr, len, buffer);

    len = 0;
    memset(buffer, '#', maxlen);
    buffer[maxlen] = 0;
    hr = pGetAcceptLanguagesA( buffer, &len);
    /* w2k3 and below: E_FAIL, since w2k8: E_INVALIDARG */
    ok((hr == E_FAIL) || (hr == E_INVALIDARG),
        "got 0x%x (expected E_FAIL or E_INVALIDARG)\n", hr);

    memset(buffer, '#', maxlen);
    buffer[maxlen] = 0;
    hr = pGetAcceptLanguagesA( buffer, NULL);
    /* w2k3 and below: E_FAIL, since w2k8: E_INVALIDARG */
    ok((hr == E_FAIL) || (hr == E_INVALIDARG),
        "got 0x%x (expected E_FAIL or E_INVALIDARG)\n", hr);


    hr = pGetAcceptLanguagesA( NULL, NULL);
    /* w2k3 and below: E_FAIL, since w2k8: E_INVALIDARG */
    ok((hr == E_FAIL) || (hr == E_INVALIDARG),
        "got 0x%x (expected E_FAIL or E_INVALIDARG)\n", hr);

restore_original:
    if (!res_query) {
        len = lstrlenA(original);
        lres = RegSetValueExA(hroot, acceptlanguage, 0, REG_SZ, (const BYTE *) original, len ? len + 1: 0);
        ok(!lres, "RegSetValueEx(%s) failed: %d\n", original, lres);
    }
    else
    {
        RegDeleteValue(hroot, acceptlanguage);
    }
    RegCloseKey(hroot);
}

static void test_SHSearchMapInt(void)
{
  int keys[8], values[8];
  int i = 0;

  if (!pSHSearchMapInt)
    return;

  memset(keys, 0, sizeof(keys));
  memset(values, 0, sizeof(values));
  keys[0] = 99; values[0] = 101;

  /* NULL key/value lists crash native, so skip testing them */

  /* 1 element */
  i = pSHSearchMapInt(keys, values, 1, keys[0]);
  ok(i == values[0], "Len 1, expected %d, got %d\n", values[0], i);

  /* Key doesn't exist */
  i = pSHSearchMapInt(keys, values, 1, 100);
  ok(i == -1, "Len 1 - bad key, expected -1, got %d\n", i);

  /* Len = 0 => not found */
  i = pSHSearchMapInt(keys, values, 0, keys[0]);
  ok(i == -1, "Len 1 - passed len 0, expected -1, got %d\n", i);

  /* 2 elements, len = 1 */
  keys[1] = 98; values[1] = 102;
  i = pSHSearchMapInt(keys, values, 1, keys[1]);
  ok(i == -1, "Len 1 - array len 2, expected -1, got %d\n", i);

  /* 2 elements, len = 2 */
  i = pSHSearchMapInt(keys, values, 2, keys[1]);
  ok(i == values[1], "Len 2, expected %d, got %d\n", values[1], i);

  /* Searches forward */
  keys[2] = 99; values[2] = 103;
  i = pSHSearchMapInt(keys, values, 3, keys[0]);
  ok(i == values[0], "Len 3, expected %d, got %d\n", values[0], i);
}

static void test_alloc_shared(void)
{
    DWORD procid;
    HANDLE hmem;
    int val;
    int* p;
    BOOL ret;

    procid=GetCurrentProcessId();
    hmem=pSHAllocShared(NULL,10,procid);
    ok(hmem!=NULL,"SHAllocShared(NULL...) failed: %u\n", GetLastError());
    ret = pSHFreeShared(hmem, procid);
    ok( ret, "SHFreeShared failed: %u\n", GetLastError());

    val=0x12345678;
    hmem=pSHAllocShared(&val,4,procid);
    ok(hmem!=NULL,"SHAllocShared(NULL...) failed: %u\n", GetLastError());

    p=pSHLockShared(hmem,procid);
    ok(p!=NULL,"SHLockShared failed: %u\n", GetLastError());
    if (p!=NULL)
        ok(*p==val,"Wrong value in shared memory: %d instead of %d\n",*p,val);
    ret = pSHUnlockShared(p);
    ok( ret, "SHUnlockShared failed: %u\n", GetLastError());

    ret = pSHFreeShared(hmem, procid);
    ok( ret, "SHFreeShared failed: %u\n", GetLastError());
}

static void test_fdsa(void)
{
    typedef struct
    {
        DWORD num_items;       /* Number of elements inserted */
        void *mem;             /* Ptr to array */
        DWORD blocks_alloced;  /* Number of elements allocated */
        BYTE inc;              /* Number of elements to grow by when we need to expand */
        BYTE block_size;       /* Size in bytes of an element */
        BYTE flags;            /* Flags */
    } FDSA_info;

    BOOL (WINAPI *pFDSA_Initialize)(DWORD block_size, DWORD inc, FDSA_info *info, void *mem,
                                    DWORD init_blocks);
    BOOL (WINAPI *pFDSA_Destroy)(FDSA_info *info);
    DWORD (WINAPI *pFDSA_InsertItem)(FDSA_info *info, DWORD where, const void *block);
    BOOL (WINAPI *pFDSA_DeleteItem)(FDSA_info *info, DWORD where);

    FDSA_info info;
    int block_size = 10, init_blocks = 4, inc = 2;
    DWORD ret;
    char *mem;

    pFDSA_Initialize = (void *)GetProcAddress(hShlwapi, (LPSTR)208);
    pFDSA_Destroy    = (void *)GetProcAddress(hShlwapi, (LPSTR)209);
    pFDSA_InsertItem = (void *)GetProcAddress(hShlwapi, (LPSTR)210);
    pFDSA_DeleteItem = (void *)GetProcAddress(hShlwapi, (LPSTR)211);

    mem = HeapAlloc(GetProcessHeap(), 0, block_size * init_blocks);
    memset(&info, 0, sizeof(info));

    ok(pFDSA_Initialize(block_size, inc, &info, mem, init_blocks), "FDSA_Initialize rets FALSE\n");
    ok(info.num_items == 0, "num_items = %d\n", info.num_items);
    ok(info.mem == mem, "mem = %p\n", info.mem);
    ok(info.blocks_alloced == init_blocks, "blocks_alloced = %d\n", info.blocks_alloced);
    ok(info.inc == inc, "inc = %d\n", info.inc);
    ok(info.block_size == block_size, "block_size = %d\n", info.block_size);
    ok(info.flags == 0, "flags = %d\n", info.flags);

    ret = pFDSA_InsertItem(&info, 1234, "1234567890");
    ok(ret == 0, "ret = %d\n", ret);
    ok(info.num_items == 1, "num_items = %d\n", info.num_items);
    ok(info.mem == mem, "mem = %p\n", info.mem);
    ok(info.blocks_alloced == init_blocks, "blocks_alloced = %d\n", info.blocks_alloced);
    ok(info.inc == inc, "inc = %d\n", info.inc);
    ok(info.block_size == block_size, "block_size = %d\n", info.block_size);
    ok(info.flags == 0, "flags = %d\n", info.flags);

    ret = pFDSA_InsertItem(&info, 1234, "abcdefghij");
    ok(ret == 1, "ret = %d\n", ret);

    ret = pFDSA_InsertItem(&info, 1, "klmnopqrst");
    ok(ret == 1, "ret = %d\n", ret);

    ret = pFDSA_InsertItem(&info, 0, "uvwxyzABCD");
    ok(ret == 0, "ret = %d\n", ret);
    ok(info.mem == mem, "mem = %p\n", info.mem);
    ok(info.flags == 0, "flags = %d\n", info.flags);

    /* This next InsertItem will cause shlwapi to allocate its own mem buffer */
    ret = pFDSA_InsertItem(&info, 0, "EFGHIJKLMN");
    ok(ret == 0, "ret = %d\n", ret);
    ok(info.mem != mem, "mem = %p\n", info.mem);
    ok(info.blocks_alloced == init_blocks + inc, "blocks_alloced = %d\n", info.blocks_alloced);
    ok(info.flags == 0x1, "flags = %d\n", info.flags);

    ok(!memcmp(info.mem, "EFGHIJKLMNuvwxyzABCD1234567890klmnopqrstabcdefghij", 50), "mem %s\n", (char*)info.mem);

    ok(pFDSA_DeleteItem(&info, 2), "rets FALSE\n");
    ok(info.mem != mem, "mem = %p\n", info.mem);
    ok(info.blocks_alloced == init_blocks + inc, "blocks_alloced = %d\n", info.blocks_alloced);
    ok(info.flags == 0x1, "flags = %d\n", info.flags);

    ok(!memcmp(info.mem, "EFGHIJKLMNuvwxyzABCDklmnopqrstabcdefghij", 40), "mem %s\n", (char*)info.mem);

    ok(pFDSA_DeleteItem(&info, 3), "rets FALSE\n");
    ok(info.mem != mem, "mem = %p\n", info.mem);
    ok(info.blocks_alloced == init_blocks + inc, "blocks_alloced = %d\n", info.blocks_alloced);
    ok(info.flags == 0x1, "flags = %d\n", info.flags);

    ok(!memcmp(info.mem, "EFGHIJKLMNuvwxyzABCDklmnopqrst", 30), "mem %s\n", (char*)info.mem);

    ok(!pFDSA_DeleteItem(&info, 4), "does not ret FALSE\n");

    /* As shlwapi has allocated memory internally, Destroy will ret FALSE */
    ok(!pFDSA_Destroy(&info), "FDSA_Destroy does not ret FALSE\n");


    /* When Initialize is called with inc = 0, set it to 1 */
    ok(pFDSA_Initialize(block_size, 0, &info, mem, init_blocks), "FDSA_Initialize rets FALSE\n");
    ok(info.inc == 1, "inc = %d\n", info.inc);

    /* This time, because shlwapi hasn't had to allocate memory
       internally, Destroy rets non-zero */
    ok(pFDSA_Destroy(&info), "FDSA_Destroy rets FALSE\n");


    HeapFree(GetProcessHeap(), 0, mem);
}


typedef struct SHELL_USER_SID {
    SID_IDENTIFIER_AUTHORITY sidAuthority;
    DWORD                    dwUserGroupID;
    DWORD                    dwUserID;
} SHELL_USER_SID, *PSHELL_USER_SID;
typedef struct SHELL_USER_PERMISSION {
    SHELL_USER_SID susID;
    DWORD          dwAccessType;
    BOOL           fInherit;
    DWORD          dwAccessMask;
    DWORD          dwInheritMask;
    DWORD          dwInheritAccessMask;
} SHELL_USER_PERMISSION, *PSHELL_USER_PERMISSION;
static void test_GetShellSecurityDescriptor(void)
{
    SHELL_USER_PERMISSION supCurrentUserFull = {
        { {SECURITY_NULL_SID_AUTHORITY}, 0, 0 },
        ACCESS_ALLOWED_ACE_TYPE, FALSE,
        GENERIC_ALL, 0, 0 };
#define MY_INHERITANCE 0xBE /* invalid value to proof behavior */
    SHELL_USER_PERMISSION supEveryoneDenied = {
        { {SECURITY_WORLD_SID_AUTHORITY}, SECURITY_WORLD_RID, 0 },
        ACCESS_DENIED_ACE_TYPE, TRUE,
        GENERIC_WRITE, MY_INHERITANCE | 0xDEADBA00, GENERIC_READ };
    PSHELL_USER_PERMISSION rgsup[2] = {
        &supCurrentUserFull, &supEveryoneDenied,
    };
    SECURITY_DESCRIPTOR* psd;
    SECURITY_DESCRIPTOR* (WINAPI*pGetShellSecurityDescriptor)(PSHELL_USER_PERMISSION*,int);

    pGetShellSecurityDescriptor=(void*)GetProcAddress(hShlwapi,(char*)475);

    if(!pGetShellSecurityDescriptor)
    {
        win_skip("GetShellSecurityDescriptor not available\n");
        return;
    }

    psd = pGetShellSecurityDescriptor(NULL, 2);
    ok(psd==NULL ||
       broken(psd==INVALID_HANDLE_VALUE), /* IE5 */
       "GetShellSecurityDescriptor should fail\n");
    psd = pGetShellSecurityDescriptor(rgsup, 0);
    ok(psd==NULL, "GetShellSecurityDescriptor should fail\n");

    SetLastError(0xdeadbeef);
    psd = pGetShellSecurityDescriptor(rgsup, 2);
    if (psd == NULL && GetLastError() == ERROR_CALL_NOT_IMPLEMENTED)
    {
        /* The previous calls to GetShellSecurityDescriptor don't set the last error */
        win_skip("GetShellSecurityDescriptor is not implemented\n");
        return;
    }
    if (psd==INVALID_HANDLE_VALUE)
    {
        win_skip("GetShellSecurityDescriptor is broken on IE5\n");
        return;
    }
    ok(psd!=NULL, "GetShellSecurityDescriptor failed\n");
    if (psd!=NULL)
    {
        BOOL bHasDacl = FALSE, bDefaulted;
        PACL pAcl;
        DWORD dwRev;
        SECURITY_DESCRIPTOR_CONTROL control;

        ok(IsValidSecurityDescriptor(psd), "returned value is not valid SD\n");

        ok(GetSecurityDescriptorControl(psd, &control, &dwRev),
                "GetSecurityDescriptorControl failed with error %u\n", GetLastError());
        ok(0 == (control & SE_SELF_RELATIVE), "SD should be absolute\n");

        ok(GetSecurityDescriptorDacl(psd, &bHasDacl, &pAcl, &bDefaulted), 
            "GetSecurityDescriptorDacl failed with error %u\n", GetLastError());

        ok(bHasDacl, "SD has no DACL\n");
        if (bHasDacl)
        {
            ok(!bDefaulted, "DACL should not be defaulted\n");

            ok(pAcl != NULL, "NULL DACL!\n");
            if (pAcl != NULL)
            {
                ACL_SIZE_INFORMATION asiSize;

                ok(IsValidAcl(pAcl), "DACL is not valid\n");

                ok(GetAclInformation(pAcl, &asiSize, sizeof(asiSize), AclSizeInformation),
                        "GetAclInformation failed with error %u\n", GetLastError());

                ok(asiSize.AceCount == 3, "Incorrect number of ACEs: %d entries\n", asiSize.AceCount);
                if (asiSize.AceCount == 3)
                {
                    ACCESS_ALLOWED_ACE *paaa; /* will use for DENIED too */

                    ok(GetAce(pAcl, 0, (LPVOID*)&paaa), "GetAce failed with error %u\n", GetLastError());
                    ok(paaa->Header.AceType == ACCESS_ALLOWED_ACE_TYPE, 
                            "Invalid ACE type %d\n", paaa->Header.AceType); 
                    ok(paaa->Header.AceFlags == 0, "Invalid ACE flags %x\n", paaa->Header.AceFlags);
                    ok(paaa->Mask == GENERIC_ALL, "Invalid ACE mask %x\n", paaa->Mask);

                    ok(GetAce(pAcl, 1, (LPVOID*)&paaa), "GetAce failed with error %u\n", GetLastError());
                    ok(paaa->Header.AceType == ACCESS_DENIED_ACE_TYPE, 
                            "Invalid ACE type %d\n", paaa->Header.AceType); 
                    /* first one of two ACEs generated from inheritable entry - without inheritance */
                    ok(paaa->Header.AceFlags == 0, "Invalid ACE flags %x\n", paaa->Header.AceFlags);
                    ok(paaa->Mask == GENERIC_WRITE, "Invalid ACE mask %x\n", paaa->Mask);

                    ok(GetAce(pAcl, 2, (LPVOID*)&paaa), "GetAce failed with error %u\n", GetLastError());
                    ok(paaa->Header.AceType == ACCESS_DENIED_ACE_TYPE, 
                            "Invalid ACE type %d\n", paaa->Header.AceType); 
                    /* second ACE - with inheritance */
                    ok(paaa->Header.AceFlags == MY_INHERITANCE,
                            "Invalid ACE flags %x\n", paaa->Header.AceFlags);
                    ok(paaa->Mask == GENERIC_READ, "Invalid ACE mask %x\n", paaa->Mask);
                }
            }
        }

        LocalFree(psd);
    }
}

static void test_SHPackDispParams(void)
{
    DISPPARAMS params;
    VARIANT vars[10];
    HRESULT hres;

    if(!pSHPackDispParams)
        win_skip("SHPackSidpParams not available\n");

    memset(&params, 0xc0, sizeof(params));
    memset(vars, 0xc0, sizeof(vars));
    hres = pSHPackDispParams(&params, vars, 1, VT_I4, 0xdeadbeef);
    ok(hres == S_OK, "SHPackDispParams failed: %08x\n", hres);
    ok(params.cArgs == 1, "params.cArgs = %d\n", params.cArgs);
    ok(params.cNamedArgs == 0, "params.cNamedArgs = %d\n", params.cArgs);
    ok(params.rgdispidNamedArgs == NULL, "params.rgdispidNamedArgs = %p\n", params.rgdispidNamedArgs);
    ok(params.rgvarg == vars, "params.rgvarg = %p\n", params.rgvarg);
    ok(V_VT(vars) == VT_I4, "V_VT(var) = %d\n", V_VT(vars));
    ok(V_I4(vars) == 0xdeadbeef, "failed %x\n", V_I4(vars));

    memset(&params, 0xc0, sizeof(params));
    hres = pSHPackDispParams(&params, NULL, 0, 0);
    ok(hres == S_OK, "SHPackDispParams failed: %08x\n", hres);
    ok(params.cArgs == 0, "params.cArgs = %d\n", params.cArgs);
    ok(params.cNamedArgs == 0, "params.cNamedArgs = %d\n", params.cArgs);
    ok(params.rgdispidNamedArgs == NULL, "params.rgdispidNamedArgs = %p\n", params.rgdispidNamedArgs);
    ok(params.rgvarg == NULL, "params.rgvarg = %p\n", params.rgvarg);

    memset(vars, 0xc0, sizeof(vars));
    memset(&params, 0xc0, sizeof(params));
    hres = pSHPackDispParams(&params, vars, 4, VT_BSTR, (void*)0xdeadbeef, VT_EMPTY, 10,
            VT_I4, 100, VT_DISPATCH, (void*)0xdeadbeef);
    ok(hres == S_OK, "SHPackDispParams failed: %08x\n", hres);
    ok(params.cArgs == 4, "params.cArgs = %d\n", params.cArgs);
    ok(params.cNamedArgs == 0, "params.cNamedArgs = %d\n", params.cArgs);
    ok(params.rgdispidNamedArgs == NULL, "params.rgdispidNamedArgs = %p\n", params.rgdispidNamedArgs);
    ok(params.rgvarg == vars, "params.rgvarg = %p\n", params.rgvarg);
    ok(V_VT(vars) == VT_DISPATCH, "V_VT(vars[0]) = %x\n", V_VT(vars));
    ok(V_I4(vars) == 0xdeadbeef, "V_I4(vars[0]) = %x\n", V_I4(vars));
    ok(V_VT(vars+1) == VT_I4, "V_VT(vars[1]) = %d\n", V_VT(vars+1));
    ok(V_I4(vars+1) == 100, "V_I4(vars[1]) = %x\n", V_I4(vars+1));
    ok(V_VT(vars+2) == VT_I4, "V_VT(vars[2]) = %d\n", V_VT(vars+2));
    ok(V_I4(vars+2) == 10, "V_I4(vars[2]) = %x\n", V_I4(vars+2));
    ok(V_VT(vars+3) == VT_BSTR, "V_VT(vars[3]) = %d\n", V_VT(vars+3));
    ok(V_BSTR(vars+3) == (void*)0xdeadbeef, "V_BSTR(vars[3]) = %p\n", V_BSTR(vars+3));
}

typedef struct _disp
{
    const IDispatchVtbl *vtbl;
    LONG   refCount;
} Disp;

typedef struct _contain
{
    const IConnectionPointContainerVtbl *vtbl;
    LONG   refCount;

    UINT  ptCount;
    IConnectionPoint **pt;
} Contain;

typedef struct _cntptn
{
    const IConnectionPointVtbl *vtbl;
    LONG refCount;

    Contain *container;
    GUID  id;
    UINT  sinkCount;
    IUnknown **sink;
} ConPt;

typedef struct _enum
{
    const IEnumConnectionsVtbl *vtbl;
    LONG   refCount;

    UINT idx;
    ConPt *pt;
} EnumCon;

typedef struct _enumpt
{
    const IEnumConnectionPointsVtbl *vtbl;
    LONG   refCount;

    int idx;
    Contain *container;
} EnumPt;


static HRESULT WINAPI Disp_QueryInterface(
        IDispatch* This,
        REFIID riid,
        void **ppvObject)
{
    *ppvObject = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDispatch))
    {
        *ppvObject = This;
    }

    if (*ppvObject)
    {
        IUnknown_AddRef(This);
        return S_OK;
    }

    trace("no interface\n");
    return E_NOINTERFACE;
}

static ULONG WINAPI Disp_AddRef(IDispatch* This)
{
    Disp *iface = (Disp*)This;
    return InterlockedIncrement(&iface->refCount);
}

static ULONG WINAPI Disp_Release(IDispatch* This)
{
    Disp *iface = (Disp*)This;
    ULONG ret;

    ret = InterlockedDecrement(&iface->refCount);
    if (ret == 0)
        HeapFree(GetProcessHeap(),0,This);
    return ret;
}

static HRESULT WINAPI Disp_GetTypeInfoCount(
        IDispatch* This,
        UINT *pctinfo)
{
    return ERROR_SUCCESS;
}

static HRESULT WINAPI Disp_GetTypeInfo(
        IDispatch* This,
        UINT iTInfo,
        LCID lcid,
        ITypeInfo **ppTInfo)
{
    return ERROR_SUCCESS;
}

static HRESULT WINAPI Disp_GetIDsOfNames(
        IDispatch* This,
        REFIID riid,
        LPOLESTR *rgszNames,
        UINT cNames,
        LCID lcid,
        DISPID *rgDispId)
{
    return ERROR_SUCCESS;
}

static HRESULT WINAPI Disp_Invoke(
        IDispatch* This,
        DISPID dispIdMember,
        REFIID riid,
        LCID lcid,
        WORD wFlags,
        DISPPARAMS *pDispParams,
        VARIANT *pVarResult,
        EXCEPINFO *pExcepInfo,
        UINT *puArgErr)
{
    trace("%p %x %p %x %x %p %p %p %p\n",This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr);

    ok(dispIdMember == 0xa0 || dispIdMember == 0xa1, "Unknown dispIdMember\n");
    ok(pDispParams != NULL, "Invoked with NULL pDispParams\n");
    ok(wFlags == DISPATCH_METHOD, "Wrong flags %x\n",wFlags);
    ok(lcid == 0,"Wrong lcid %x\n",lcid);
    if (dispIdMember == 0xa0)
    {
        ok(pDispParams->cArgs == 0, "params.cArgs = %d\n", pDispParams->cArgs);
        ok(pDispParams->cNamedArgs == 0, "params.cNamedArgs = %d\n", pDispParams->cArgs);
        ok(pDispParams->rgdispidNamedArgs == NULL, "params.rgdispidNamedArgs = %p\n", pDispParams->rgdispidNamedArgs);
        ok(pDispParams->rgvarg == NULL, "params.rgvarg = %p\n", pDispParams->rgvarg);
    }
    else if (dispIdMember == 0xa1)
    {
        ok(pDispParams->cArgs == 2, "params.cArgs = %d\n", pDispParams->cArgs);
        ok(pDispParams->cNamedArgs == 0, "params.cNamedArgs = %d\n", pDispParams->cArgs);
        ok(pDispParams->rgdispidNamedArgs == NULL, "params.rgdispidNamedArgs = %p\n", pDispParams->rgdispidNamedArgs);
        ok(V_VT(pDispParams->rgvarg) == VT_BSTR, "V_VT(var) = %d\n", V_VT(pDispParams->rgvarg));
        ok(V_I4(pDispParams->rgvarg) == 0xdeadcafe , "failed %p\n", V_BSTR(pDispParams->rgvarg));
        ok(V_VT(pDispParams->rgvarg+1) == VT_I4, "V_VT(var) = %d\n", V_VT(pDispParams->rgvarg+1));
        ok(V_I4(pDispParams->rgvarg+1) == 0xdeadbeef, "failed %x\n", V_I4(pDispParams->rgvarg+1));
    }

    return ERROR_SUCCESS;
}

static const IDispatchVtbl disp_vtbl = {
    Disp_QueryInterface,
    Disp_AddRef,
    Disp_Release,

    Disp_GetTypeInfoCount,
    Disp_GetTypeInfo,
    Disp_GetIDsOfNames,
    Disp_Invoke
};

static HRESULT WINAPI Enum_QueryInterface(
        IEnumConnections* This,
        REFIID riid,
        void **ppvObject)
{
    *ppvObject = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IEnumConnections))
    {
        *ppvObject = This;
    }

    if (*ppvObject)
    {
        IUnknown_AddRef(This);
        return S_OK;
    }

    trace("no interface\n");
    return E_NOINTERFACE;
}

static ULONG WINAPI Enum_AddRef(IEnumConnections* This)
{
    EnumCon *iface = (EnumCon*)This;
    return InterlockedIncrement(&iface->refCount);
}

static ULONG WINAPI Enum_Release(IEnumConnections* This)
{
    EnumCon *iface = (EnumCon*)This;
    ULONG ret;

    ret = InterlockedDecrement(&iface->refCount);
    if (ret == 0)
        HeapFree(GetProcessHeap(),0,This);
    return ret;
}

static HRESULT WINAPI Enum_Next(
        IEnumConnections* This,
        ULONG cConnections,
        LPCONNECTDATA rgcd,
        ULONG *pcFetched)
{
    EnumCon *iface = (EnumCon*)This;

    if (cConnections > 0 && iface->idx < iface->pt->sinkCount)
    {
        rgcd->pUnk = iface->pt->sink[iface->idx];
        IUnknown_AddRef(iface->pt->sink[iface->idx]);
        rgcd->dwCookie=0xff;
        if (pcFetched)
            *pcFetched = 1;
        iface->idx++;
        return S_OK;
    }

    return E_FAIL;
}

static HRESULT WINAPI Enum_Skip(
        IEnumConnections* This,
        ULONG cConnections)
{
    return E_FAIL;
}

static HRESULT WINAPI Enum_Reset(
        IEnumConnections* This)
{
    return E_FAIL;
}

static HRESULT WINAPI Enum_Clone(
        IEnumConnections* This,
        IEnumConnections **ppEnum)
{
    return E_FAIL;
}

static const IEnumConnectionsVtbl enum_vtbl = {

    Enum_QueryInterface,
    Enum_AddRef,
    Enum_Release,
    Enum_Next,
    Enum_Skip,
    Enum_Reset,
    Enum_Clone
};

static HRESULT WINAPI ConPt_QueryInterface(
        IConnectionPoint* This,
        REFIID riid,
        void **ppvObject)
{
    *ppvObject = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IConnectionPoint))
    {
        *ppvObject = This;
    }

    if (*ppvObject)
    {
        IUnknown_AddRef(This);
        return S_OK;
    }

    trace("no interface\n");
    return E_NOINTERFACE;
}

static ULONG WINAPI ConPt_AddRef(
        IConnectionPoint* This)
{
    ConPt *iface = (ConPt*)This;
    return InterlockedIncrement(&iface->refCount);
}

static ULONG WINAPI ConPt_Release(
        IConnectionPoint* This)
{
    ConPt *iface = (ConPt*)This;
    ULONG ret;

    ret = InterlockedDecrement(&iface->refCount);
    if (ret == 0)
    {
        if (iface->sinkCount > 0)
        {
            int i;
            for (i = 0; i < iface->sinkCount; i++)
            {
                if (iface->sink[i])
                    IUnknown_Release(iface->sink[i]);
            }
            HeapFree(GetProcessHeap(),0,iface->sink);
        }
        HeapFree(GetProcessHeap(),0,This);
    }
    return ret;
}

static HRESULT WINAPI ConPt_GetConnectionInterface(
        IConnectionPoint* This,
        IID *pIID)
{
    static int i = 0;
    ConPt *iface = (ConPt*)This;
    if (i==0)
    {
        i++;
        return E_FAIL;
    }
    else
        memcpy(pIID,&iface->id,sizeof(GUID));
    return S_OK;
}

static HRESULT WINAPI ConPt_GetConnectionPointContainer(
        IConnectionPoint* This,
        IConnectionPointContainer **ppCPC)
{
    ConPt *iface = (ConPt*)This;

    *ppCPC = (IConnectionPointContainer*)iface->container;
    return S_OK;
}

static HRESULT WINAPI ConPt_Advise(
        IConnectionPoint* This,
        IUnknown *pUnkSink,
        DWORD *pdwCookie)
{
    ConPt *iface = (ConPt*)This;

    if (iface->sinkCount == 0)
        iface->sink = HeapAlloc(GetProcessHeap(),0,sizeof(IUnknown*));
    else
        iface->sink = HeapReAlloc(GetProcessHeap(),0,iface->sink,sizeof(IUnknown*)*(iface->sinkCount+1));
    iface->sink[iface->sinkCount] = pUnkSink;
    IUnknown_AddRef(pUnkSink);
    iface->sinkCount++;
    *pdwCookie = iface->sinkCount;
    return S_OK;
}

static HRESULT WINAPI ConPt_Unadvise(
        IConnectionPoint* This,
        DWORD dwCookie)
{
    ConPt *iface = (ConPt*)This;

    if (dwCookie > iface->sinkCount)
        return E_FAIL;
    else
    {
        IUnknown_Release(iface->sink[dwCookie-1]);
        iface->sink[dwCookie-1] = NULL;
    }
    return S_OK;
}

static HRESULT WINAPI ConPt_EnumConnections(
        IConnectionPoint* This,
        IEnumConnections **ppEnum)
{
    EnumCon *ec;

    ec = HeapAlloc(GetProcessHeap(),0,sizeof(EnumCon));
    ec->vtbl = &enum_vtbl;
    ec->refCount = 1;
    ec->pt = (ConPt*)This;
    ec->idx = 0;
    *ppEnum = (IEnumConnections*)ec;

    return S_OK;
}

static const IConnectionPointVtbl point_vtbl = {
    ConPt_QueryInterface,
    ConPt_AddRef,
    ConPt_Release,

    ConPt_GetConnectionInterface,
    ConPt_GetConnectionPointContainer,
    ConPt_Advise,
    ConPt_Unadvise,
    ConPt_EnumConnections
};

static HRESULT WINAPI EnumPt_QueryInterface(
        IEnumConnectionPoints* This,
        REFIID riid,
        void **ppvObject)
{
    *ppvObject = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IEnumConnectionPoints))
    {
        *ppvObject = This;
    }

    if (*ppvObject)
    {
        IUnknown_AddRef(This);
        return S_OK;
    }

    trace("no interface\n");
    return E_NOINTERFACE;
}

static ULONG WINAPI EnumPt_AddRef(IEnumConnectionPoints* This)
{
    EnumPt *iface = (EnumPt*)This;
    return InterlockedIncrement(&iface->refCount);
}

static ULONG WINAPI EnumPt_Release(IEnumConnectionPoints* This)
{
    EnumPt *iface = (EnumPt*)This;
    ULONG ret;

    ret = InterlockedDecrement(&iface->refCount);
    if (ret == 0)
        HeapFree(GetProcessHeap(),0,This);
    return ret;
}

static HRESULT WINAPI EnumPt_Next(
        IEnumConnectionPoints* This,
        ULONG cConnections,
        IConnectionPoint **rgcd,
        ULONG *pcFetched)
{
    EnumPt *iface = (EnumPt*)This;

    if (cConnections > 0 && iface->idx < iface->container->ptCount)
    {
        *rgcd = iface->container->pt[iface->idx];
        IUnknown_AddRef(iface->container->pt[iface->idx]);
        if (pcFetched)
            *pcFetched = 1;
        iface->idx++;
        return S_OK;
    }

    return E_FAIL;
}

static HRESULT WINAPI EnumPt_Skip(
        IEnumConnectionPoints* This,
        ULONG cConnections)
{
    return E_FAIL;
}

static HRESULT WINAPI EnumPt_Reset(
        IEnumConnectionPoints* This)
{
    return E_FAIL;
}

static HRESULT WINAPI EnumPt_Clone(
        IEnumConnectionPoints* This,
        IEnumConnectionPoints **ppEnumPt)
{
    return E_FAIL;
}

static const IEnumConnectionPointsVtbl enumpt_vtbl = {

    EnumPt_QueryInterface,
    EnumPt_AddRef,
    EnumPt_Release,
    EnumPt_Next,
    EnumPt_Skip,
    EnumPt_Reset,
    EnumPt_Clone
};

static HRESULT WINAPI Contain_QueryInterface(
        IConnectionPointContainer* This,
        REFIID riid,
        void **ppvObject)
{
    *ppvObject = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IConnectionPointContainer))
    {
        *ppvObject = This;
    }

    if (*ppvObject)
    {
        IUnknown_AddRef(This);
        return S_OK;
    }

    trace("no interface\n");
    return E_NOINTERFACE;
}

static ULONG WINAPI Contain_AddRef(
        IConnectionPointContainer* This)
{
    Contain *iface = (Contain*)This;
    return InterlockedIncrement(&iface->refCount);
}

static ULONG WINAPI Contain_Release(
        IConnectionPointContainer* This)
{
    Contain *iface = (Contain*)This;
    ULONG ret;

    ret = InterlockedDecrement(&iface->refCount);
    if (ret == 0)
    {
        if (iface->ptCount > 0)
        {
            int i;
            for (i = 0; i < iface->ptCount; i++)
                IUnknown_Release(iface->pt[i]);
            HeapFree(GetProcessHeap(),0,iface->pt);
        }
        HeapFree(GetProcessHeap(),0,This);
    }
    return ret;
}

static HRESULT WINAPI Contain_EnumConnectionPoints(
        IConnectionPointContainer* This,
        IEnumConnectionPoints **ppEnum)
{
    EnumPt *ec;

    ec = HeapAlloc(GetProcessHeap(),0,sizeof(EnumPt));
    ec->vtbl = &enumpt_vtbl;
    ec->refCount = 1;
    ec->idx= 0;
    ec->container = (Contain*)This;
    *ppEnum = (IEnumConnectionPoints*)ec;

    return S_OK;
}

static HRESULT WINAPI Contain_FindConnectionPoint(
        IConnectionPointContainer* This,
        REFIID riid,
        IConnectionPoint **ppCP)
{
    Contain *iface = (Contain*)This;
    ConPt *pt;

    if (!IsEqualIID(riid, &IID_NULL) || iface->ptCount ==0)
    {
        pt = HeapAlloc(GetProcessHeap(),0,sizeof(ConPt));
        pt->vtbl = &point_vtbl;
        pt->refCount = 1;
        pt->sinkCount = 0;
        pt->sink = NULL;
        pt->container = iface;
        pt->id = IID_IDispatch;

        if (iface->ptCount == 0)
            iface->pt =HeapAlloc(GetProcessHeap(),0,sizeof(IUnknown*));
        else
            iface->pt = HeapReAlloc(GetProcessHeap(),0,iface->pt,sizeof(IUnknown*)*(iface->ptCount+1));
        iface->pt[iface->ptCount] = (IConnectionPoint*)pt;
        iface->ptCount++;

        *ppCP = (IConnectionPoint*)pt;
    }
    else
    {
        *ppCP = iface->pt[0];
        IUnknown_AddRef((IUnknown*)*ppCP);
    }

    return S_OK;
}

static const IConnectionPointContainerVtbl contain_vtbl = {
    Contain_QueryInterface,
    Contain_AddRef,
    Contain_Release,

    Contain_EnumConnectionPoints,
    Contain_FindConnectionPoint
};

static void test_IConnectionPoint(void)
{
    HRESULT rc;
    ULONG ref;
    IConnectionPoint *point;
    Contain *container;
    Disp *dispatch;
    DWORD cookie = 0xffffffff;
    DISPPARAMS params;
    VARIANT vars[10];

    if (!pIConnectionPoint_SimpleInvoke || !pConnectToConnectionPoint)
    {
        win_skip("IConnectionPoint Apis not present\n");
        return;
    }

    container = HeapAlloc(GetProcessHeap(),0,sizeof(Contain));
    container->vtbl = &contain_vtbl;
    container->refCount = 1;
    container->ptCount = 0;
    container->pt = NULL;

    dispatch = HeapAlloc(GetProcessHeap(),0,sizeof(Disp));
    dispatch->vtbl = &disp_vtbl;
    dispatch->refCount = 1;

    rc = pConnectToConnectionPoint((IUnknown*)dispatch, &IID_NULL, TRUE, (IUnknown*)container, &cookie, &point);
    ok(rc == S_OK, "pConnectToConnectionPoint failed with %x\n",rc);
    ok(point != NULL, "returned ConnectionPoint is NULL\n");
    ok(cookie != 0xffffffff, "invalid cookie returned\n");

    rc = pIConnectionPoint_SimpleInvoke(point,0xa0,NULL);
    ok(rc == S_OK, "pConnectToConnectionPoint failed with %x\n",rc);

    if (pSHPackDispParams)
    {
        memset(&params, 0xc0, sizeof(params));
        memset(vars, 0xc0, sizeof(vars));
        rc = pSHPackDispParams(&params, vars, 2, VT_I4, 0xdeadbeef, VT_BSTR, 0xdeadcafe);
        ok(rc == S_OK, "SHPackDispParams failed: %08x\n", rc);

        rc = pIConnectionPoint_SimpleInvoke(point,0xa1,&params);
        ok(rc == S_OK, "pConnectToConnectionPoint failed with %x\n",rc);
    }
    else
        win_skip("pSHPackDispParams not present\n");

    rc = pConnectToConnectionPoint(NULL, &IID_NULL, FALSE, (IUnknown*)container, &cookie, NULL);
    ok(rc == S_OK, "pConnectToConnectionPoint failed with %x\n",rc);

/* MSDN says this should be required but it crashs on XP
    IUnknown_Release(point);
*/
    ref = IUnknown_Release((IUnknown*)container);
    ok(ref == 0, "leftover IConnectionPointContainer reference %i\n",ref);
    ref = IUnknown_Release((IUnknown*)dispatch);
    ok(ref == 0, "leftover IDispatch reference %i\n",ref);
}

typedef struct _propbag
{
    const IPropertyBagVtbl *vtbl;
    LONG   refCount;

} PropBag;


static HRESULT WINAPI Prop_QueryInterface(
        IPropertyBag* This,
        REFIID riid,
        void **ppvObject)
{
    *ppvObject = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IPropertyBag))
    {
        *ppvObject = This;
    }

    if (*ppvObject)
    {
        IUnknown_AddRef(This);
        return S_OK;
    }

    trace("no interface\n");
    return E_NOINTERFACE;
}

static ULONG WINAPI Prop_AddRef(
        IPropertyBag* This)
{
    PropBag *iface = (PropBag*)This;
    return InterlockedIncrement(&iface->refCount);
}

static ULONG WINAPI Prop_Release(
        IPropertyBag* This)
{
    PropBag *iface = (PropBag*)This;
    ULONG ret;

    ret = InterlockedDecrement(&iface->refCount);
    if (ret == 0)
        HeapFree(GetProcessHeap(),0,This);
    return ret;
}

static HRESULT WINAPI Prop_Read(
        IPropertyBag* This,
        LPCOLESTR pszPropName,
        VARIANT *pVar,
        IErrorLog *pErrorLog)
{
    V_VT(pVar) = VT_BLOB|VT_BYREF;
    V_BYREF(pVar) = (LPVOID)0xdeadcafe;
    return S_OK;
}

static HRESULT WINAPI Prop_Write(
        IPropertyBag* This,
        LPCOLESTR pszPropName,
        VARIANT *pVar)
{
    return S_OK;
}


static const IPropertyBagVtbl prop_vtbl = {
    Prop_QueryInterface,
    Prop_AddRef,
    Prop_Release,

    Prop_Read,
    Prop_Write
};

static void test_SHPropertyBag_ReadLONG(void)
{
    PropBag *pb;
    HRESULT rc;
    LONG out;
    static const WCHAR szName1[] = {'n','a','m','e','1',0};

    if (!pSHPropertyBag_ReadLONG)
    {
        win_skip("SHPropertyBag_ReadLONG not present\n");
        return;
    }

    pb = HeapAlloc(GetProcessHeap(),0,sizeof(PropBag));
    pb->refCount = 1;
    pb->vtbl = &prop_vtbl;

    out = 0xfeedface;
    rc = pSHPropertyBag_ReadLONG(NULL, szName1, &out);
    ok(rc == E_INVALIDARG || broken(rc == 0), "incorrect return %x\n",rc);
    ok(out == 0xfeedface, "value should not have changed\n");
    rc = pSHPropertyBag_ReadLONG((IPropertyBag*)pb, NULL, &out);
    ok(rc == E_INVALIDARG || broken(rc == 0) || broken(rc == 1), "incorrect return %x\n",rc);
    ok(out == 0xfeedface, "value should not have changed\n");
    rc = pSHPropertyBag_ReadLONG((IPropertyBag*)pb, szName1, NULL);
    ok(rc == E_INVALIDARG || broken(rc == 0) || broken(rc == 1), "incorrect return %x\n",rc);
    ok(out == 0xfeedface, "value should not have changed\n");
    rc = pSHPropertyBag_ReadLONG((IPropertyBag*)pb, szName1, &out);
    ok(rc == DISP_E_BADVARTYPE || broken(rc == 0) || broken(rc == 1), "incorrect return %x\n",rc);
    ok(out == 0xfeedface  || broken(out == 0xfeedfa00), "value should not have changed %x\n",out);
    IUnknown_Release((IUnknown*)pb);
}



static void test_SHSetWindowBits(void)
{
    HWND hwnd;
    DWORD style, styleold;
    WNDCLASSA clsA;

    if(!pSHSetWindowBits)
    {
        win_skip("SHSetWindowBits is not available\n");
        return;
    }

    clsA.style = 0;
    clsA.lpfnWndProc = DefWindowProcA;
    clsA.cbClsExtra = 0;
    clsA.cbWndExtra = 0;
    clsA.hInstance = GetModuleHandleA(NULL);
    clsA.hIcon = 0;
    clsA.hCursor = LoadCursorA(0, IDC_ARROW);
    clsA.hbrBackground = NULL;
    clsA.lpszMenuName = NULL;
    clsA.lpszClassName = "Shlwapi test class";
    RegisterClassA(&clsA);

    hwnd = CreateWindowA("Shlwapi test class", "Test", WS_VISIBLE, 0, 0, 100, 100,
                          NULL, NULL, GetModuleHandle(NULL), 0);
    ok(IsWindow(hwnd), "failed to create window\n");

    /* null window */
    SetLastError(0xdeadbeef);
    style = pSHSetWindowBits(NULL, GWL_STYLE, 0, 0);
    ok(style == 0, "expected 0 retval, got %d\n", style);
    ok(GetLastError() == ERROR_INVALID_WINDOW_HANDLE ||
        broken(GetLastError() == 0xdeadbeef), /* Win9x/WinMe */
        "expected ERROR_INVALID_WINDOW_HANDLE, got %d\n", GetLastError());

    /* zero mask, zero flags */
    styleold = GetWindowLongA(hwnd, GWL_STYLE);
    style = pSHSetWindowBits(hwnd, GWL_STYLE, 0, 0);
    ok(styleold == style, "expected old style\n");
    ok(styleold == GetWindowLongA(hwnd, GWL_STYLE), "expected to keep old style\n");

    /* test mask */
    styleold = GetWindowLongA(hwnd, GWL_STYLE);
    ok(styleold & WS_VISIBLE, "expected WS_VISIBLE\n");
    style = pSHSetWindowBits(hwnd, GWL_STYLE, WS_VISIBLE, 0);

    ok(style == styleold, "expected previous style, got %x\n", style);
    ok((GetWindowLongA(hwnd, GWL_STYLE) & WS_VISIBLE) == 0, "expected updated style\n");

    /* test mask, unset style bit used */
    styleold = GetWindowLongA(hwnd, GWL_STYLE);
    style = pSHSetWindowBits(hwnd, GWL_STYLE, WS_VISIBLE, 0);
    ok(style == styleold, "expected previous style, got %x\n", style);
    ok(styleold == GetWindowLongA(hwnd, GWL_STYLE), "expected to keep old style\n");

    /* set back with flags */
    styleold = GetWindowLongA(hwnd, GWL_STYLE);
    style = pSHSetWindowBits(hwnd, GWL_STYLE, WS_VISIBLE, WS_VISIBLE);
    ok(style == styleold, "expected previous style, got %x\n", style);
    ok(GetWindowLongA(hwnd, GWL_STYLE) & WS_VISIBLE, "expected updated style\n");

    /* reset and try to set without a mask */
    pSHSetWindowBits(hwnd, GWL_STYLE, WS_VISIBLE, 0);
    ok((GetWindowLongA(hwnd, GWL_STYLE) & WS_VISIBLE) == 0, "expected updated style\n");
    styleold = GetWindowLongA(hwnd, GWL_STYLE);
    style = pSHSetWindowBits(hwnd, GWL_STYLE, 0, WS_VISIBLE);
    ok(style == styleold, "expected previous style, got %x\n", style);
    ok((GetWindowLongA(hwnd, GWL_STYLE) & WS_VISIBLE) == 0, "expected updated style\n");

    DestroyWindow(hwnd);

    UnregisterClassA("Shlwapi test class", GetModuleHandleA(NULL));
}

static void test_SHFormatDateTimeA(void)
{
    FILETIME UNALIGNED filetime;
    CHAR buff[100], buff2[100], buff3[100];
    SYSTEMTIME st;
    DWORD flags;
    INT ret;

    if(!pSHFormatDateTimeA)
    {
        win_skip("pSHFormatDateTimeA isn't available\n");
        return;
    }

if (0)
{
    /* crashes on native */
    ret = pSHFormatDateTimeA(NULL, NULL, NULL, 0);
}

    GetLocalTime(&st);
    SystemTimeToFileTime(&st, &filetime);
    /* SHFormatDateTime expects input as utc */
    LocalFileTimeToFileTime(&filetime, &filetime);

    /* no way to get required buffer length here */
    SetLastError(0xdeadbeef);
    ret = pSHFormatDateTimeA(&filetime, NULL, NULL, 0);
    ok(ret == 0, "got %d\n", ret);
    ok(GetLastError() == 0xdeadbeef, "expected 0xdeadbeef, got %d\n", GetLastError());

    SetLastError(0xdeadbeef);
    buff[0] = 'a'; buff[1] = 0;
    ret = pSHFormatDateTimeA(&filetime, NULL, buff, 0);
    ok(ret == 0, "got %d\n", ret);
    ok(GetLastError() == 0xdeadbeef, "expected 0xdeadbeef, got %d\n", GetLastError());
    ok(buff[0] == 'a', "expected same string, got %s\n", buff);

    /* all combinations documented as invalid succeeded */
    flags = FDTF_SHORTTIME | FDTF_LONGTIME;
    SetLastError(0xdeadbeef);
    ret = pSHFormatDateTimeA(&filetime, &flags, buff, sizeof(buff));
    ok(ret == lstrlenA(buff)+1, "got %d\n", ret);
    ok(GetLastError() == 0xdeadbeef, "expected 0xdeadbeef, got %d\n", GetLastError());

    flags = FDTF_SHORTDATE | FDTF_LONGDATE;
    SetLastError(0xdeadbeef);
    ret = pSHFormatDateTimeA(&filetime, &flags, buff, sizeof(buff));
    ok(ret == lstrlenA(buff)+1, "got %d\n", ret);
    ok(GetLastError() == 0xdeadbeef, "expected 0xdeadbeef, got %d\n", GetLastError());

    flags = FDTF_SHORTDATE | FDTF_LTRDATE | FDTF_RTLDATE;
    SetLastError(0xdeadbeef);
    ret = pSHFormatDateTimeA(&filetime, &flags, buff, sizeof(buff));
    ok(ret == lstrlenA(buff)+1, "got %d\n", ret);
    ok(GetLastError() == 0xdeadbeef ||
        broken(GetLastError() == ERROR_INVALID_FLAGS), /* Win9x/WinMe */
        "expected 0xdeadbeef, got %d\n", GetLastError());

    /* now check returned strings */
    flags = FDTF_SHORTTIME;
    ret = pSHFormatDateTimeA(&filetime, &flags, buff, sizeof(buff));
    ok(ret == lstrlenA(buff)+1, "got %d\n", ret);
    ret = GetTimeFormat(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &st, NULL, buff2, sizeof(buff2));
    ok(ret == lstrlenA(buff2)+1, "got %d\n", ret);
    ok(lstrcmpA(buff, buff2) == 0, "expected (%s), got (%s)\n", buff2, buff);

    flags = FDTF_LONGTIME;
    ret = pSHFormatDateTimeA(&filetime, &flags, buff, sizeof(buff));
    ok(ret == lstrlenA(buff)+1, "got %d\n", ret);
    ret = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, buff2, sizeof(buff2));
    ok(ret == lstrlenA(buff2)+1, "got %d\n", ret);
    ok(lstrcmpA(buff, buff2) == 0, "expected (%s), got (%s)\n", buff2, buff);

    /* both time flags */
    flags = FDTF_LONGTIME | FDTF_SHORTTIME;
    ret = pSHFormatDateTimeA(&filetime, &flags, buff, sizeof(buff));
    ok(ret == lstrlenA(buff)+1, "got %d\n", ret);
    ret = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, buff2, sizeof(buff2));
    ok(ret == lstrlenA(buff2)+1, "got %d\n", ret);
    ok(lstrcmpA(buff, buff2) == 0, "expected (%s), got (%s)\n", buff2, buff);

    flags = FDTF_SHORTDATE;
    ret = pSHFormatDateTimeA(&filetime, &flags, buff, sizeof(buff));
    ok(ret == lstrlenA(buff)+1, "got %d\n", ret);
    ret = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, buff2, sizeof(buff2));
    ok(ret == lstrlenA(buff2)+1, "got %d\n", ret);
    ok(lstrcmpA(buff, buff2) == 0, "expected (%s), got (%s)\n", buff2, buff);

    flags = FDTF_LONGDATE;
    ret = pSHFormatDateTimeA(&filetime, &flags, buff, sizeof(buff));
    ok(ret == lstrlenA(buff)+1, "got %d\n", ret);
    ret = GetDateFormat(LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, NULL, buff2, sizeof(buff2));
    ok(ret == lstrlenA(buff2)+1, "got %d\n", ret);
    ok(lstrcmpA(buff, buff2) == 0, "expected (%s), got (%s)\n", buff2, buff);

    /* both date flags */
    flags = FDTF_LONGDATE | FDTF_SHORTDATE;
    ret = pSHFormatDateTimeA(&filetime, &flags, buff, sizeof(buff));
    ok(ret == lstrlenA(buff)+1, "got %d\n", ret);
    ret = GetDateFormat(LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, NULL, buff2, sizeof(buff2));
    ok(ret == lstrlenA(buff2)+1, "got %d\n", ret);
    ok(lstrcmpA(buff, buff2) == 0, "expected (%s), got (%s)\n", buff2, buff);

    /* various combinations of date/time flags */
    flags = FDTF_LONGDATE | FDTF_SHORTTIME;
    ret = pSHFormatDateTimeA(&filetime, &flags, buff, sizeof(buff));
    ok(ret == lstrlenA(buff)+1, "got %d, length %d\n", ret, lstrlenA(buff)+1);
    ret = GetDateFormat(LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, NULL, buff2, sizeof(buff2));
    ok(ret == lstrlenA(buff2)+1, "got %d\n", ret);
    strcat(buff2, ", ");
    ret = GetTimeFormat(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &st, NULL, buff3, sizeof(buff3));
    ok(ret == lstrlenA(buff3)+1, "got %d\n", ret);
    strcat(buff2, buff3);
    ok(lstrcmpA(buff, buff2) == 0, "expected (%s), got (%s)\n", buff2, buff);

    flags = FDTF_LONGDATE | FDTF_LONGTIME;
    ret = pSHFormatDateTimeA(&filetime, &flags, buff, sizeof(buff));
    ok(ret == lstrlenA(buff)+1, "got %d\n", ret);
    ret = GetDateFormat(LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, NULL, buff2, sizeof(buff2));
    ok(ret == lstrlenA(buff2)+1, "got %d\n", ret);
    strcat(buff2, ", ");
    ret = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, buff3, sizeof(buff3));
    ok(ret == lstrlenA(buff3)+1, "got %d\n", ret);
    strcat(buff2, buff3);
    ok(lstrcmpA(buff, buff2) == 0, "expected (%s), got (%s)\n", buff2, buff);

    flags = FDTF_SHORTDATE | FDTF_SHORTTIME;
    ret = pSHFormatDateTimeA(&filetime, &flags, buff, sizeof(buff));
    ok(ret == lstrlenA(buff)+1, "got %d\n", ret);
    ret = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, buff2, sizeof(buff2));
    ok(ret == lstrlenA(buff2)+1, "got %d\n", ret);
    strcat(buff2, " ");
    ret = GetTimeFormat(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &st, NULL, buff3, sizeof(buff3));
    ok(ret == lstrlenA(buff3)+1, "got %d\n", ret);
    strcat(buff2, buff3);
    ok(lstrcmpA(buff, buff2) == 0, "expected (%s), got (%s)\n", buff2, buff);

    flags = FDTF_SHORTDATE | FDTF_LONGTIME;
    ret = pSHFormatDateTimeA(&filetime, &flags, buff, sizeof(buff));
    ok(ret == lstrlenA(buff)+1, "got %d\n", ret);
    ret = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, buff2, sizeof(buff2));
    ok(ret == lstrlenA(buff2)+1, "got %d\n", ret);
    strcat(buff2, " ");
    ret = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, buff3, sizeof(buff3));
    ok(ret == lstrlenA(buff3)+1, "got %d\n", ret);
    strcat(buff2, buff3);
    ok(lstrcmpA(buff, buff2) == 0, "expected (%s), got (%s)\n", buff2, buff);
}

static void test_SHFormatDateTimeW(void)
{
    FILETIME UNALIGNED filetime;
    WCHAR buff[100], buff2[100], buff3[100];
    SYSTEMTIME st;
    DWORD flags;
    INT ret;
    static const WCHAR spaceW[] = {' ',0};
    static const WCHAR commaW[] = {',',' ',0};

    if(!pSHFormatDateTimeW)
    {
        win_skip("pSHFormatDateTimeW isn't available\n");
        return;
    }

if (0)
{
    /* crashes on native */
    ret = pSHFormatDateTimeW(NULL, NULL, NULL, 0);
}

    GetLocalTime(&st);
    SystemTimeToFileTime(&st, &filetime);
    /* SHFormatDateTime expects input as utc */
    LocalFileTimeToFileTime(&filetime, &filetime);

    /* no way to get required buffer length here */
    SetLastError(0xdeadbeef);
    ret = pSHFormatDateTimeW(&filetime, NULL, NULL, 0);
    ok(ret == 0, "got %d\n", ret);
    ok(GetLastError() == 0xdeadbeef, "expected 0xdeadbeef, got %d\n", GetLastError());

    SetLastError(0xdeadbeef);
    buff[0] = 'a'; buff[1] = 0;
    ret = pSHFormatDateTimeW(&filetime, NULL, buff, 0);
    ok(ret == 0, "got %d\n", ret);
    ok(GetLastError() == 0xdeadbeef, "expected 0xdeadbeef, got %d\n", GetLastError());
    ok(buff[0] == 'a', "expected same string\n");

    /* all combinations documented as invalid succeeded */
    flags = FDTF_SHORTTIME | FDTF_LONGTIME;
    SetLastError(0xdeadbeef);
    ret = pSHFormatDateTimeW(&filetime, &flags, buff, sizeof(buff)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff)+1, "got %d\n", ret);
    ok(GetLastError() == 0xdeadbeef, "expected 0xdeadbeef, got %d\n", GetLastError());

    flags = FDTF_SHORTDATE | FDTF_LONGDATE;
    SetLastError(0xdeadbeef);
    ret = pSHFormatDateTimeW(&filetime, &flags, buff, sizeof(buff)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff)+1, "got %d\n", ret);
    ok(GetLastError() == 0xdeadbeef, "expected 0xdeadbeef, got %d\n", GetLastError());

    flags = FDTF_SHORTDATE | FDTF_LTRDATE | FDTF_RTLDATE;
    SetLastError(0xdeadbeef);
    buff[0] = 0; /* NT4 doesn't clear the buffer on failure */
    ret = pSHFormatDateTimeW(&filetime, &flags, buff, sizeof(buff)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff)+1, "got %d\n", ret);
    ok(GetLastError() == 0xdeadbeef ||
        broken(GetLastError() == ERROR_INVALID_FLAGS), /* Win9x/WinMe/NT4 */
        "expected 0xdeadbeef, got %d\n", GetLastError());

    /* now check returned strings */
    flags = FDTF_SHORTTIME;
    ret = pSHFormatDateTimeW(&filetime, &flags, buff, sizeof(buff)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff)+1, "got %d\n", ret);
    SetLastError(0xdeadbeef);
    ret = GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &st, NULL, buff2, sizeof(buff2)/sizeof(WCHAR));
    if (ret == 0 && GetLastError() == ERROR_CALL_NOT_IMPLEMENTED)
    {
        win_skip("Needed W-functions are not implemented\n");
        return;
    }
    ok(ret == lstrlenW(buff2)+1, "got %d\n", ret);
    ok(lstrcmpW(buff, buff2) == 0, "expected equal strings\n");

    flags = FDTF_LONGTIME;
    ret = pSHFormatDateTimeW(&filetime, &flags, buff, sizeof(buff)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff)+1, "got %d\n", ret);
    ret = GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &st, NULL, buff2, sizeof(buff2)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff2)+1, "got %d\n", ret);
    ok(lstrcmpW(buff, buff2) == 0, "expected equal strings\n");

    /* both time flags */
    flags = FDTF_LONGTIME | FDTF_SHORTTIME;
    ret = pSHFormatDateTimeW(&filetime, &flags, buff, sizeof(buff)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff)+1, "got %d\n", ret);
    ret = GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &st, NULL, buff2, sizeof(buff2)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff2)+1, "got %d\n", ret);
    ok(lstrcmpW(buff, buff2) == 0, "expected equal string\n");

    flags = FDTF_SHORTDATE;
    ret = pSHFormatDateTimeW(&filetime, &flags, buff, sizeof(buff)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff)+1, "got %d\n", ret);
    ret = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, buff2, sizeof(buff2)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff2)+1, "got %d\n", ret);
    ok(lstrcmpW(buff, buff2) == 0, "expected equal strings\n");

    flags = FDTF_LONGDATE;
    ret = pSHFormatDateTimeW(&filetime, &flags, buff, sizeof(buff)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff)+1, "got %d\n", ret);
    ret = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, NULL, buff2, sizeof(buff2)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff2)+1, "got %d\n", ret);
    ok(lstrcmpW(buff, buff2) == 0, "expected equal strings\n");

    /* both date flags */
    flags = FDTF_LONGDATE | FDTF_SHORTDATE;
    ret = pSHFormatDateTimeW(&filetime, &flags, buff, sizeof(buff)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff)+1, "got %d\n", ret);
    ret = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, NULL, buff2, sizeof(buff2)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff2)+1, "got %d\n", ret);
    ok(lstrcmpW(buff, buff2) == 0, "expected equal strings\n");

    /* various combinations of date/time flags */
    flags = FDTF_LONGDATE | FDTF_SHORTTIME;
    ret = pSHFormatDateTimeW(&filetime, &flags, buff, sizeof(buff)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff)+1, "got %d, length %d\n", ret, lstrlenW(buff)+1);
    ret = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, NULL, buff2, sizeof(buff2)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff2)+1, "got %d\n", ret);
    lstrcatW(buff2, commaW);
    ret = GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &st, NULL, buff3, sizeof(buff3)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff3)+1, "got %d\n", ret);
    lstrcatW(buff2, buff3);
    ok(lstrcmpW(buff, buff2) == 0, "expected equal strings\n");

    flags = FDTF_LONGDATE | FDTF_LONGTIME;
    ret = pSHFormatDateTimeW(&filetime, &flags, buff, sizeof(buff)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff)+1, "got %d\n", ret);
    ret = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, NULL, buff2, sizeof(buff2)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff2)+1, "got %d\n", ret);
    lstrcatW(buff2, commaW);
    ret = GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &st, NULL, buff3, sizeof(buff3)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff3)+1, "got %d\n", ret);
    lstrcatW(buff2, buff3);
    ok(lstrcmpW(buff, buff2) == 0, "expected equal strings\n");

    flags = FDTF_SHORTDATE | FDTF_SHORTTIME;
    ret = pSHFormatDateTimeW(&filetime, &flags, buff, sizeof(buff)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff)+1, "got %d\n", ret);
    ret = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, buff2, sizeof(buff2)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff2)+1, "got %d\n", ret);
    lstrcatW(buff2, spaceW);
    ret = GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &st, NULL, buff3, sizeof(buff3)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff3)+1, "got %d\n", ret);
    lstrcatW(buff2, buff3);
    ok(lstrcmpW(buff, buff2) == 0, "expected equal strings\n");

    flags = FDTF_SHORTDATE | FDTF_LONGTIME;
    ret = pSHFormatDateTimeW(&filetime, &flags, buff, sizeof(buff)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff)+1, "got %d\n", ret);
    ret = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, buff2, sizeof(buff2)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff2)+1, "got %d\n", ret);
    lstrcatW(buff2, spaceW);
    ret = GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &st, NULL, buff3, sizeof(buff3)/sizeof(WCHAR));
    ok(ret == lstrlenW(buff3)+1, "got %d\n", ret);
    lstrcatW(buff2, buff3);
    ok(lstrcmpW(buff, buff2) == 0, "expected equal strings\n");
}

static void test_SHGetObjectCompatFlags(void)
{
    struct compat_value {
        CHAR nameA[30];
        DWORD value;
    };

    struct compat_value values[] = {
        { "OTNEEDSSFCACHE", 0x1 },
        { "NO_WEBVIEW", 0x2 },
        { "UNBINDABLE", 0x4 },
        { "PINDLL", 0x8 },
        { "NEEDSFILESYSANCESTOR", 0x10 },
        { "NOTAFILESYSTEM", 0x20 },
        { "CTXMENU_NOVERBS", 0x40 },
        { "CTXMENU_LIMITEDQI", 0x80 },
        { "COCREATESHELLFOLDERONLY", 0x100 },
        { "NEEDSSTORAGEANCESTOR", 0x200 },
        { "NOLEGACYWEBVIEW", 0x400 },
        { "CTXMENU_XPQCMFLAGS", 0x1000 },
        { "NOIPROPERTYSTORE", 0x2000 }
    };

    static const char compat_path[] = "Software\\Microsoft\\Windows\\CurrentVersion\\ShellCompatibility\\Objects";
    CHAR keyA[39]; /* {CLSID} */
    HKEY root;
    DWORD ret;
    int i;

    if (!pSHGetObjectCompatFlags)
    {
        win_skip("SHGetObjectCompatFlags isn't available\n");
        return;
    }

    /* null args */
    ret = pSHGetObjectCompatFlags(NULL, NULL);
    ok(ret == 0, "got %d\n", ret);

    ret = RegOpenKeyA(HKEY_LOCAL_MACHINE, compat_path, &root);
    if (ret != ERROR_SUCCESS)
    {
        skip("No compatibility class data found\n");
        return;
    }

    for (i = 0; RegEnumKeyA(root, i, keyA, sizeof(keyA)) == ERROR_SUCCESS; i++)
    {
        HKEY clsid_key;

        if (RegOpenKeyA(root, keyA, &clsid_key) == ERROR_SUCCESS)
        {
            CHAR valueA[30];
            DWORD expected = 0, got, length = sizeof(valueA);
            CLSID clsid;
            int v;

            for (v = 0; RegEnumValueA(clsid_key, v, valueA, &length, NULL, NULL, NULL, NULL) == ERROR_SUCCESS; v++)
            {
                int j;

                for (j = 0; j < sizeof(values)/sizeof(struct compat_value); j++)
                    if (lstrcmpA(values[j].nameA, valueA) == 0)
                    {
                        expected |= values[j].value;
                        break;
                    }

                length = sizeof(valueA);
            }

            pGUIDFromStringA(keyA, &clsid);
            got = pSHGetObjectCompatFlags(NULL, &clsid);
            ok(got == expected, "got 0x%08x, expected 0x%08x. Key %s\n", got, expected, keyA);

            RegCloseKey(clsid_key);
        }
    }

    RegCloseKey(root);
}

static void init_pointers(void)
{
#define MAKEFUNC(f, ord) (p##f = (void*)GetProcAddress(hShlwapi, (LPSTR)(ord)))
    MAKEFUNC(SHAllocShared, 7);
    MAKEFUNC(SHLockShared, 8);
    MAKEFUNC(SHUnlockShared, 9);
    MAKEFUNC(SHFreeShared, 10);
    MAKEFUNC(GetAcceptLanguagesA, 14);
    MAKEFUNC(SHSetWindowBits, 165);
    MAKEFUNC(ConnectToConnectionPoint, 168);
    MAKEFUNC(SHSearchMapInt, 198);
    MAKEFUNC(GUIDFromStringA, 269);
    MAKEFUNC(SHPackDispParams, 282);
    MAKEFUNC(IConnectionPoint_InvokeWithCancel, 283);
    MAKEFUNC(IConnectionPoint_SimpleInvoke, 284);
    MAKEFUNC(SHFormatDateTimeA, 353);
    MAKEFUNC(SHFormatDateTimeW, 354);
    MAKEFUNC(SHGetObjectCompatFlags, 476);
    MAKEFUNC(SHPropertyBag_ReadLONG, 496);
#undef MAKEFUNC
}

START_TEST(ordinal)
{
    hShlwapi = GetModuleHandleA("shlwapi.dll");

    init_pointers();

    hmlang = LoadLibraryA("mlang.dll");
    pLcidToRfc1766A = (void *)GetProcAddress(hmlang, "LcidToRfc1766A");

    test_GetAcceptLanguagesA();
    test_SHSearchMapInt();
    test_alloc_shared();
    test_fdsa();
    test_GetShellSecurityDescriptor();
    test_SHPackDispParams();
    test_IConnectionPoint();
    test_SHPropertyBag_ReadLONG();
    test_SHSetWindowBits();
    test_SHFormatDateTimeA();
    test_SHFormatDateTimeW();
    test_SHGetObjectCompatFlags();
}
