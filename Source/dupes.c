// $Id$

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h> // Common controls
#include <shellapi.h>
#include <shlobj.h>   // Directory dialog
#include <stdio.h>    // sprintf
#include <stdlib.h>
#include <malloc.h>   // malloc, realloc
#include <search.h>   // qsort
#include "resource.h"
// md5 stuff
#include "md5c.cpp"
#define MD 5
//include "global.h"
//include "md5.h"
#define MD_CTX MD5_CTX
#define MDInit MD5Init
#define MDUpdate MD5Update
#define MDFinal MD5Final

#define APPNAME "Dupes"
#define MBAPPERROR MB_OK | MB_ICONERROR
#define IDM_ABOUTMENU 5000

HINSTANCE hMainInst;
HWND hMainWnd;
HWND g_hStatus, g_hProgress, g_hList;
HANDLE g_hThread;
int i;
typedef struct {
	char *filename;
	unsigned int filesize;
	char *checksum;
} aFile;

int g_nFoundFiles, g_fileArraySize;
aFile *g_files;

int g_nFoundFilesNew, g_fileArraySizeNew;
aFile *g_filesNew;

LRESULT CALLBACK MainDlgHandler(HWND, UINT, WPARAM, LPARAM);
LONG MainThread(LPVOID);
void ResizeWindowByHwnd(HWND);
void ResizeWindow(HWND, int, int);
int __cdecl SortFiles(const void *, const void *);
void FindFiles(char *);
BOOL PathExists(char *);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	INITCOMMONCONTROLSEX cc;
	cc.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
	cc.dwSize = sizeof(cc);
	InitCommonControlsEx(&cc);
	hMainInst = hInstance;
	g_hThread = NULL;
	DialogBox(GetModuleHandle(NULL), "MAIN", 0, (DLGPROC)MainDlgHandler);
	return 0;
}

LRESULT CALLBACK MainDlgHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	LPNMHDR hdr;
	LVITEM *pItem;
	LVCOLUMN col;
	LPITEMIDLIST pidl;
	BROWSEINFO bi;
	TCHAR szTemp[MAX_PATH];
	HMENU sysMenu;
	int ret, i, nDeleted;
	switch (message) {
	case WM_INITDIALOG:
		hMainWnd = hwnd;
		sysMenu = GetSystemMenu(hwnd, FALSE);
		if (sysMenu != NULL) {
			AppendMenu(sysMenu, MF_SEPARATOR, (int)NULL, NULL);
			AppendMenu(sysMenu, MF_STRING, IDM_ABOUTMENU, "&About...");
		}
		DrawMenuBar(hwnd);
		g_hList = GetDlgItem(hwnd, IDL_FILES);
		g_hProgress = GetDlgItem(hwnd, IDC_PROGRESS);
		ListView_SetExtendedListViewStyle(g_hList, LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);
		col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
		col.fmt = LVCFMT_LEFT;
		col.pszText = "Filename";
		col.cx = 350;
		ListView_InsertColumn(g_hList, 0, &col);
		col.fmt = LVCFMT_RIGHT;
		col.pszText = "Size";
		col.cx = 80;
		ListView_InsertColumn(g_hList, 1, &col);
		col.fmt = LVCFMT_LEFT;
		col.pszText = "Checksum";
		col.cx = 220;
		ListView_InsertColumn(g_hList, 2, &col);
		g_hStatus = CreateWindowEx(0, STATUSCLASSNAME, (LPCTSTR)NULL, WS_CHILD | WS_VISIBLE,
			0, 0, 0, 0, hwnd, 0, hMainInst, NULL);
		ResizeWindowByHwnd(hwnd);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BROWSE:
			ZeroMemory(&bi, sizeof(bi));
			bi.hwndOwner = hwnd;
			bi.pidlRoot = NULL;
			bi.pszDisplayName = NULL;
			bi.lpszTitle = "Please select the directory which contains the files you would like to check for duplicates.";
			bi.ulFlags = BIF_RETURNONLYFSDIRS;
			bi.lParam = (long)NULL;
			bi.iImage = 0;
			pidl = SHBrowseForFolder(&bi);
			if (pidl != NULL) {
				if (SHGetPathFromIDList(pidl, szTemp))
					SetDlgItemText(hwnd, IDT_DIRECTORY, szTemp);
			}
			return TRUE;
		case IDOK:
			GetDlgItemText(hwnd, IDT_DIRECTORY, szTemp, sizeof(szTemp));
			if (!PathExists(szTemp)) {
				MessageBox(hwnd, "Please choose a valid path to scan for dupes.", APPNAME, MBAPPERROR);
				return TRUE;
			}
			SendMessage(g_hStatus, WM_SETTEXT, (WPARAM)NULL, (LPARAM)"Scanning...");
			g_hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)MainThread, (LPVOID)0, 0, 0);
			return TRUE;
		case IDC_DELETE:
			ret = MessageBox(hwnd, "Are you sure you want to do this? "
				"The selected files will be deleted permanently, without going into the recycle bin.",
				APPNAME, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
			if (ret == IDYES) {
				nDeleted = 0;
				g_nFoundFilesNew = 0;
				g_fileArraySizeNew = g_nFoundFiles;
				g_filesNew = malloc(sizeof(aFile) * g_nFoundFiles);
				for (i = 0; i < g_nFoundFiles; i++) {
					if (ListView_GetItemState(g_hList, i, LVIS_SELECTED) == LVIS_SELECTED) {
						ListView_SetItemState(g_hList, i, 0, LVIS_SELECTED);
						DeleteFile(g_files[i].filename);
						nDeleted++;
						free(g_files[i].filename);
						free(g_files[i].checksum);
					}
					else {
						g_filesNew[g_nFoundFilesNew] = g_files[i];
						g_nFoundFilesNew++;
					}
				}
				g_nFoundFiles = g_nFoundFilesNew;
				g_fileArraySize = g_fileArraySizeNew;
				free(g_files);
				g_files = g_filesNew;
				SendMessage(g_hList, LVM_SETITEMCOUNT, (WPARAM)g_nFoundFiles, (LPARAM)LVSICF_NOSCROLL);
				sprintf(szTemp, "%d files found", g_nFoundFiles);
				SendMessage(g_hStatus, SB_SETTEXT, (WPARAM)1, (LPARAM)szTemp);
				sprintf(szTemp, "%d files deleted.", nDeleted);
				MessageBox(hwnd, szTemp, APPNAME, MB_OK | MB_ICONINFORMATION);
			}
			return TRUE;
		}
	case WM_NOTIFY:
		switch (wParam) {
		case IDL_FILES:
			hdr = (LPNMHDR)lParam;
			switch (hdr->code) {
			case LVN_GETDISPINFO:
				pItem = &((NMLVDISPINFO*)lParam)->item;
				if (pItem->mask & LVIF_TEXT) {
					switch (pItem->iSubItem) {
					case 0:
						lstrcpy(pItem->pszText, g_files[pItem->iItem].filename);
						break;
					case 1:
						sprintf(pItem->pszText, "%d", g_files[pItem->iItem].filesize);
						break;
					case 2:
						lstrcpy(pItem->pszText, g_files[pItem->iItem].checksum);
						break;
					}
				}
			}
		}
	case WM_SYSCOMMAND:
		switch (wParam) {
		case IDM_ABOUTMENU:
			MessageBox(hwnd, "Dupes 1.0\nhttp://www.bolt.cx/dupes/\nCopyright © 2002 - Chris Bolt",
				APPNAME, MB_OK | MB_ICONINFORMATION);
			return TRUE;
		case SC_CLOSE:
			EndDialog(hwnd, TRUE);
			return TRUE;
		default:
			return FALSE;
		}
	case WM_SIZE:
		SendMessage(g_hStatus, WM_SIZE, wParam, lParam);
		ResizeWindow(hwnd, LOWORD(lParam), HIWORD(lParam));
		return TRUE;
	}
	return FALSE;
}

LONG MainThread(LPVOID lpThreadData) {
	int len, j;
	char szTemp[MAX_PATH];
	FILE *fp;
	MD_CTX context;
	unsigned char szBuffer[1024], szDigest[16], szAscDigest[32];
	EnableWindow(GetDlgItem(hMainWnd, IDL_FILES), TRUE);
	EnableWindow(GetDlgItem(hMainWnd, IDC_DELETE), FALSE);
	SendMessage(g_hProgress, PBM_SETPOS, 0, 0);
	GetDlgItemText(hMainWnd, IDT_DIRECTORY, szTemp, sizeof(szTemp));
	if (szTemp[strlen(szTemp) - 1] == '\\')
		strcat(szTemp, "*");
	else
		strcat(szTemp, "\\*");
	g_nFoundFiles = 0;
	g_fileArraySize = 16;
	g_files = malloc(sizeof(aFile) * g_fileArraySize);
	FindFiles(szTemp);
	sprintf(szTemp, "Removing files with unique file sizes...");
	SendMessage(g_hStatus, WM_SETTEXT, (WPARAM)NULL, (LPARAM)szTemp);

	qsort(g_files, g_nFoundFiles, sizeof(aFile), SortFiles);
	SendMessage(g_hList, LVM_SETITEMCOUNT, (WPARAM)g_nFoundFiles,
		(LPARAM)LVSICF_NOSCROLL);
	g_nFoundFilesNew = 0;
	g_fileArraySizeNew = 16;
	g_filesNew = malloc(sizeof(aFile) * g_fileArraySizeNew);
	for (i = 0; i < g_nFoundFiles; i++) {
		if ((i > 0 && g_files[i - 1].filesize == g_files[i].filesize) ||
			(i < g_nFoundFiles - 1 && g_files[i + 1].filesize == g_files[i].filesize)) {
			g_nFoundFilesNew++;
			if (g_nFoundFilesNew > g_fileArraySizeNew) {
				g_fileArraySizeNew *= 2;
				g_filesNew = realloc(g_filesNew, sizeof(aFile) * g_fileArraySizeNew);
			}
			g_filesNew[g_nFoundFilesNew - 1] = g_files[i];
		} else {
			free(g_files[i].filename);
			g_files[i].filename = "";
		}
	}
	free(g_files);
	g_nFoundFiles = g_nFoundFilesNew;
	g_fileArraySize = g_fileArraySizeNew;
	g_files = g_filesNew;
	SendMessage(g_hList, LVM_SETITEMCOUNT, (WPARAM)g_nFoundFiles, (LPARAM)LVSICF_NOSCROLL);
	sprintf(szTemp, "Calculating checksum for remaining files...");
	SendMessage(g_hStatus, WM_SETTEXT, (WPARAM)NULL, (LPARAM)szTemp);
	sprintf(szTemp, "%d files found", g_nFoundFiles);
	SendMessage(g_hStatus, SB_SETTEXT, (WPARAM)1, (LPARAM)szTemp);

	SendMessage(g_hProgress, PBM_SETRANGE, 0, (LPARAM)MAKELPARAM(0, g_nFoundFiles));
	for (i = 0; i < g_nFoundFiles; i++) {
		SendMessage(g_hProgress, PBM_SETPOS, (WPARAM)i, 0);
		sprintf(szTemp, "Calculating checksum for %s...", g_files[i].filename);
		SendMessage(g_hStatus, WM_SETTEXT, (WPARAM)NULL, (LPARAM)szTemp);
		if ((fp = fopen(g_files[i].filename, "rb")) == NULL)
			g_files[i].checksum = "Error";
		else {
			MDInit(&context);
			while (len = fread(szBuffer, 1, 1024, fp))
				MDUpdate(&context, szBuffer, len);
			MDFinal(szDigest, &context);
			fclose(fp);
			strcpy(szAscDigest, "");
			for (j = 0; j < 16; j++)
				sprintf(szAscDigest, "%s%02x", szAscDigest, szDigest[j]);
			g_files[i].checksum = _strdup(szAscDigest);
		}
		SendMessage(g_hList, LVM_SETITEMCOUNT, (WPARAM)g_nFoundFiles, (LPARAM)LVSICF_NOSCROLL);
	}
	SendMessage(g_hProgress, PBM_SETPOS, (WPARAM)g_nFoundFiles, 0);
	SendMessage(g_hList, LVM_SETITEMCOUNT, (WPARAM)g_nFoundFiles, (LPARAM)LVSICF_NOSCROLL);
	SendMessage(g_hStatus, WM_SETTEXT, (WPARAM)NULL, (LPARAM)"Removing files with unique checksums...");

	qsort(g_files, g_nFoundFiles, sizeof(aFile), SortFiles);
	g_nFoundFilesNew = 0;
	g_fileArraySizeNew = 16;
	g_filesNew = malloc(sizeof(aFile) * g_fileArraySizeNew);
	for (i = 0; i < g_nFoundFiles; i++) {
		if (
			(i > 0 &&
			 g_files[i - 1].filesize == g_files[i].filesize &&
			 strcmp(g_files[i - 1].checksum, g_files[i].checksum) == 0) ||
			(i < g_nFoundFiles - 1 &&
			 g_files[i + 1].filesize == g_files[i].filesize &&
			 strcmp(g_files[i + 1].checksum, g_files[i].checksum) == 0)) {
			g_nFoundFilesNew++;
			if (g_nFoundFilesNew > g_fileArraySizeNew) {
				g_fileArraySizeNew *= 2;
				g_filesNew = realloc(g_filesNew, sizeof(aFile) * g_fileArraySizeNew);
			}
			g_filesNew[g_nFoundFilesNew - 1] = g_files[i];
		} else {
			free(g_files[i].filename);
			g_files[i].filename = "";
			free(g_files[i].checksum);
			g_files[i].checksum = "";
		}
	}
	free(g_files);
	g_nFoundFiles = g_nFoundFilesNew;
	g_fileArraySize = g_fileArraySizeNew;
	g_files = g_filesNew;
	sprintf(szTemp, "%d files found", g_nFoundFiles);
	SendMessage(g_hStatus, SB_SETTEXT, (WPARAM)1, (LPARAM)szTemp);

	for (i = 0; i < g_nFoundFiles; i++) {
		if (i > 0 && g_files[i - 1].filesize == g_files[i].filesize)
			ListView_SetItemState(g_hList, i, LVIS_SELECTED, LVIS_SELECTED);
	}
	SendMessage(g_hList, LVM_SETITEMCOUNT, (WPARAM)g_nFoundFiles, (LPARAM)LVSICF_NOSCROLL);
	sprintf(szTemp, "Done.", g_nFoundFiles);
	SendMessage(g_hStatus, WM_SETTEXT, (WPARAM)NULL, (LPARAM)szTemp);
	SetFocus(g_hList);
	if (g_nFoundFiles > 0)
		EnableWindow(GetDlgItem(hMainWnd, IDC_DELETE), TRUE);
	return 0;
}

void ResizeWindowByHwnd(HWND hwnd) {
	RECT rc;
	GetClientRect(hwnd, &rc);
	ResizeWindow(hwnd, rc.right, rc.bottom);
}

void ResizeWindow(HWND hwnd, int width, int height) {
	RECT rc;
	int parts[2], btnwidth, btnheight;
	parts[0] = width - 150;
	parts[1] = -1;
	SendMessage(g_hStatus, SB_SETPARTS, (WPARAM)2, (LPARAM)parts);
	GetWindowRect(g_hStatus, &rc);
	height -= rc.bottom - rc.top;
	GetWindowRect(GetDlgItem(hwnd, IDOK), &rc);
	btnwidth  = rc.right - rc.left;
	btnheight = rc.bottom - rc.top;
	GetWindowRect(GetDlgItem(hwnd, IDC_PROGRESS), &rc);
	MoveWindow(GetDlgItem(hwnd, IDT_DIRECTORY), 68, 13,
		width - (btnwidth * 2) - 93, btnheight - 3, TRUE);
	MoveWindow(GetDlgItem(hwnd, IDC_BROWSE), width - (btnwidth * 2) - 16, 11,
		btnwidth, btnheight, TRUE);
	MoveWindow(GetDlgItem(hwnd, IDOK), width - btnwidth - 10, 11,
		btnwidth, btnheight, TRUE);
	MoveWindow(g_hList, 11, btnheight + 19,
		width - 21, height - (btnheight * 2) - 34, TRUE);
	MoveWindow(GetDlgItem(hwnd, IDC_PROGRESS), 11, height - btnheight - 8,
		width - btnwidth - 27, btnheight, TRUE);
	MoveWindow(GetDlgItem(hwnd, IDC_DELETE), width - btnwidth - 10, height - btnheight - 8,
		btnwidth, btnheight, TRUE);
}

int __cdecl SortFiles(const void *file1, const void *file2) {
	int size1, size2;
	char *checksum1, *checksum2;
	size1 = ((aFile*)file1)->filesize;
	size2 = ((aFile*)file2)->filesize;
	if (size1 != size2)
		return (size1 > size2) ? 1 : -1;
	checksum1 = ((aFile*)file1)->checksum;
	checksum2 = ((aFile*)file2)->checksum;
	return _stricmp(checksum1, checksum2);
}

void FindFiles(char *szPath) {
	HANDLE hSearch;
	WIN32_FIND_DATA fd;
	char szTemp[MAX_PATH], szStatus[MAX_PATH];
	HANDLE fh;
	hSearch = FindFirstFile(szPath, &fd);
	do {
		if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
			strcpy(szTemp, szPath);
			szTemp[strlen(szTemp) - 2] = '\0';
			strcat(szTemp, "\\");
			strcat(szTemp, fd.cFileName);
			sprintf(szStatus, "Scanning %s...", szTemp);
			SendMessage(g_hStatus, WM_SETTEXT, (WPARAM)NULL, (LPARAM)szStatus);
			strcat(szTemp, "\\*");
			FindFiles(szTemp);
		}
		else {
			if (GetLastError() == 5) continue; // Access denied...
			g_nFoundFiles++;
			sprintf(szTemp, "%d files found", g_nFoundFiles);
			SendMessage(g_hStatus, SB_SETTEXT, (WPARAM)1, (LPARAM)szTemp);
			if (g_nFoundFiles > g_fileArraySize) {
				g_fileArraySize *= 2;
				g_files = realloc(g_files, sizeof(aFile) * g_fileArraySize);
			}
			strcpy(szTemp, szPath);
			szTemp[strlen(szTemp) - 2] = '\0';
			strcat(szTemp, "\\");
			strcat(szTemp, fd.cFileName);
			g_files[g_nFoundFiles - 1].filename = _strdup(szTemp);
			fh = CreateFile(szTemp, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, (DWORD)NULL, NULL);
			g_files[g_nFoundFiles - 1].filesize = GetFileSize(fh, NULL);
			CloseHandle(fh);
			g_files[g_nFoundFiles - 1].checksum = "";
			SendMessage(g_hList, LVM_SETITEMCOUNT, (WPARAM)g_nFoundFiles,
				(LPARAM)(LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL));
			UpdateWindow(hMainWnd);
		}
	} while (FindNextFile(hSearch, &fd));
}

BOOL PathExists(char *szPath) {
	HANDLE hSearch;
	WIN32_FIND_DATA fd;
	char szMyPath[MAX_PATH];
	if (strlen(szPath) == 0)
		return FALSE;
	strcpy(szMyPath, szPath);
	if (szMyPath[strlen(szMyPath) - 1] == '\\')
		strcat(szMyPath, "*");
	else
		strcat(szMyPath, "\\*");
	hSearch = FindFirstFile(szMyPath, &fd);
	if (hSearch == INVALID_HANDLE_VALUE)
		return FALSE;
	return TRUE;
}
