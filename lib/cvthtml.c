/**
 * @file lib/cvthtml.c
 *
 * Convert VT100/ANSI escape sequences into HTML.
 *
 * Copyright (c) 2015-2018 Malcolm J. Smith
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "yoripch.h"
#include "yorilib.h"

/**
 The default color to use when all else fails.
 */
#define CVTVT_DEFAULT_COLOR (7)

/**
 The dialect of HTML to use.
 */
DWORD YoriLibHtmlVersion = 5;

/**
 While parsing, TRUE if a tag is currently open so that it can be closed on
 the next font change.
 */
BOOLEAN YoriLibHtmlTagOpen = FALSE;

/**
 While parsing, TRUE if underlining is in effect.
 */
BOOLEAN YoriLibHtmlUnderlineOn = FALSE;

/**
 On systems that support it, populated with font information from the console
 so the output display can match the input.
 */
YORI_CONSOLE_FONT_INFOEX YoriLibHtmlFontInfo = {0};

/**
 Attempt to capture the current console font.  This is only available on
 newer systems.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriLibCaptureConsoleFont()
{
    HANDLE hConsole;
    if (DllKernel32.pGetCurrentConsoleFontEx == NULL) {
        return FALSE;
    }

    hConsole = CreateFile(_T("CONOUT$"),
                          GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          NULL,
                          OPEN_EXISTING,
                          0,
                          NULL);

    if (hConsole == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    YoriLibHtmlFontInfo.cbSize = sizeof(YoriLibHtmlFontInfo);

    if (!DllKernel32.pGetCurrentConsoleFontEx(hConsole, FALSE, &YoriLibHtmlFontInfo)) {
        CloseHandle(hConsole);
        return FALSE;
    }

    CloseHandle(hConsole);
    return TRUE;
}

/**
 Output to include at the beginning of any HTML stream.
 */
CONST TCHAR YoriLibHtmlHeader[] = _T("<HTML><HEAD><TITLE>cvtvt output</TITLE></HEAD>");

/**
 Output to include at the beginning of any version 4 HTML body.
 */
CONST TCHAR YoriLibVer4Header[] = _T("<BODY>")
                                _T("<DIV STYLE=\"background-color: #000000; font-family: ");

/**
 Output to include at the beginning of any version 5 HTML body.
 */
CONST TCHAR YoriLibVer5Header[] = _T("<BODY><DIV STYLE=\"background-color: #000000; color: #c0c0c0; border-style: ridge; border-width: 4px; display: inline-block; font-family: ");

/**
 The end of the HTML body section, after font information has been populated.
 This is used for version 4 and 5.
 */
CONST TCHAR YoriLibVerHeaderEnd[] = _T(";\">");

/**
 Final text to output at the end of any HTML stream.
 */
CONST TCHAR YoriLibFooter[] = _T("</DIV></BODY></HTML>");

/**
 Generate a string of text for the current console font and state to commence
 an HTML output stream.

 @param TextString On successful completion, updated to contain the start of
        an HTML stream.  Note this string maybe allocated in this function.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriLibHtmlGenerateInitialString(
    __inout PYORI_STRING TextString
    )
{
    TCHAR szFontNames[LF_FACESIZE + 100];
    TCHAR szFontWeight[100];
    TCHAR szFontSize[100];

    YoriLibCaptureConsoleFont();

    if (YoriLibHtmlFontInfo.FaceName[0] != '\0') {
        YoriLibSPrintf(szFontNames, _T("'%ls', monospace"), YoriLibHtmlFontInfo.FaceName);
    } else {
        YoriLibSPrintf(szFontNames, _T("monospace"));
    }

    if (YoriLibHtmlFontInfo.FontWeight == 0) {
        YoriLibHtmlFontInfo.FontWeight = 700;
    }
    if (YoriLibHtmlFontInfo.dwFontSize.Y == 0) {
        YoriLibHtmlFontInfo.dwFontSize.Y = 12;
    }

    YoriLibSPrintf(szFontSize, _T("; font-size: %ipx"), YoriLibHtmlFontInfo.dwFontSize.Y);
    if (YoriLibHtmlVersion == 4) {
        szFontWeight[0] = '\0';
    } else {
        YoriLibSPrintf(szFontWeight, _T("; font-weight: %i"), YoriLibHtmlFontInfo.FontWeight);
    }

    YoriLibYPrintf(TextString,
                   _T("%s%s%s%s%s%s%s"),
                   YoriLibHtmlHeader,
                   (YoriLibHtmlVersion == 4)?YoriLibVer4Header:YoriLibVer5Header,
                   szFontNames,
                   (YoriLibHtmlVersion == 5)?szFontWeight:_T(""),
                   szFontSize,
                   YoriLibVerHeaderEnd,
                   (YoriLibHtmlVersion == 4 && YoriLibHtmlFontInfo.FontWeight >= 600)?_T("<B>"):_T(""));

    if (TextString->StartOfString == NULL) {
        return FALSE;
    }

    return TRUE;
}

/**
 Generate a string of text for the current console font and state to end
 an HTML output stream.

 @param TextString On successful completion, updated to contain the end of
        an HTML stream.  Note this string maybe allocated in this function.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriLibHtmlGenerateEndString(
    __inout PYORI_STRING TextString
    )
{
    YoriLibYPrintf(TextString,
                   _T("%s%s%s%s"),
                   (YoriLibHtmlTagOpen?(YoriLibHtmlUnderlineOn?_T("</U>"):_T("")):_T("")),
                   (YoriLibHtmlTagOpen?(YoriLibHtmlVersion == 4?_T("</FONT>"):_T("</SPAN>")):_T("")),
                   (YoriLibHtmlVersion == 4 && YoriLibHtmlFontInfo.FontWeight >= 600)?_T("</B>"):_T(""),
                   YoriLibFooter);

    if (TextString->StartOfString == NULL) {
        return FALSE;
    }

    return TRUE;
}

/**
 Generate a string of text that encodes regular text for inclusion in HTML.

 @param TextString On successful completion, updated to contain the escaped
        HTML text.  If this buffer is not large enough, the routine succeeds
        and returns the required size in BufferSizeNeeded.

 @param BufferSizeNeeded On successful completion, indicates the number of
        characters needed in the TextString buffer to complete the operation.

 @param StringBuffer Pointer to the string containing a VT100 escape to
        convert to HTML.

 @param BufferLength Specifies the number of characters in StringBuffer.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriLibHtmlGenerateTextString(
    __inout PYORI_STRING TextString,
    __out PDWORD BufferSizeNeeded,
    __in LPTSTR StringBuffer,
    __in DWORD BufferLength
    )
{

    TCHAR  LookFor[] = {'<', '>', ' ', '\n', '\r', '\0'};
    LPTSTR SrcPoint;
    DWORD  SrcOffset = 0;
    DWORD  SrcConsumed = 0;
    DWORD  DestOffset = 0;
    YORI_STRING SearchString;

    //
    //  Scan through the string again looking for text that needs to be
    //  escaped in HTML, in particular greater than and less than since
    //  those denote tags
    //

    TextString->LengthInChars = 0;
    SrcPoint = StringBuffer;

    do {
        SearchString.StartOfString = SrcPoint;
        SearchString.LengthInChars = BufferLength - SrcConsumed;
        SrcOffset = YoriLibCountStringNotContainingChars(&SearchString, LookFor);
        if (SrcOffset > 0) {
            if (DestOffset + SrcOffset < TextString->LengthAllocated) {
                memcpy(&TextString->StartOfString[DestOffset], SrcPoint, SrcOffset * sizeof(TCHAR));
            }

            DestOffset += SrcOffset;

            SrcPoint = SrcPoint + SrcOffset;
            SrcConsumed += SrcOffset;
        }

        if (SrcConsumed < BufferLength) {

            //
            //  Escape things that could be tags, and parse escape sequences
            //  we understand
            //
    
            if (*SrcPoint == '<') {
                if (DestOffset + sizeof("&lt;") - 1 < TextString->LengthAllocated) {
                    memcpy(&TextString->StartOfString[DestOffset], _T("&lt;"), sizeof(_T("&lt;")) - sizeof(TCHAR));
                }
                DestOffset += sizeof("&lt;") - 1;
                SrcPoint++;
                SrcConsumed++;
            } else if (*SrcPoint == '>') {
                if (DestOffset + sizeof("&gt;") - 1 < TextString->LengthAllocated) {
                    memcpy(&TextString->StartOfString[DestOffset], _T("&gt;"), sizeof(_T("&gt;")) - sizeof(TCHAR));
                }
                DestOffset += sizeof("&gt;") - 1;
                SrcPoint++;
                SrcConsumed++;
            } else if (*SrcPoint == '\n') {
                if (DestOffset + sizeof("<BR>") - 1 < TextString->LengthAllocated) {
                    memcpy(&TextString->StartOfString[DestOffset], _T("<BR>"), sizeof(_T("<BR>")) - sizeof(TCHAR));
                }
                DestOffset += sizeof("<BR>") - 1;
                SrcPoint++;
                SrcConsumed++;
            } else if (*SrcPoint == ' ') {
                if (DestOffset + sizeof("&nbsp;") - 1 < TextString->LengthAllocated) {
                    memcpy(&TextString->StartOfString[DestOffset], _T("&nbsp;"), sizeof(_T("&nbsp;")) - sizeof(TCHAR));
                }
                DestOffset += sizeof("&nbsp;") - 1;
                SrcPoint++;
                SrcConsumed++;
            } else if (*SrcPoint == '\r') {
                SrcPoint++;
                SrcConsumed++;
            }
        }

    } while (SrcConsumed < BufferLength);

    if (DestOffset < TextString->LengthAllocated) {
        TextString->StartOfString[DestOffset] = '\0';
        TextString->LengthInChars = DestOffset;
    }

    *BufferSizeNeeded = DestOffset + 1;

    return TRUE;
}

/**
 Generate a string of text that descibes a VT100 escape action in terms of
 HTML.

 @param TextString On successful completion, updated to contain the escaped
        HTML text.  If this buffer is not large enough, the routine succeeds
        and returns the required size in BufferSizeNeeded.

 @param BufferSizeNeeded On successful completion, indicates the number of
        characters needed in the TextString buffer to complete the operation.

 @param StringBuffer Pointer to the string containing a VT100 escape to
        convert to HTML.

 @param BufferLength Specifies the number of characters in StringBuffer.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriLibHtmlGenerateEscapeString(
    __inout PYORI_STRING TextString,
    __out PDWORD BufferSizeNeeded,
    __in LPTSTR StringBuffer,
    __in DWORD BufferLength
    )
{
    LPTSTR SrcPoint = StringBuffer;
    DWORD  SrcOffset = 0;
    WORD   NewColor = CVTVT_DEFAULT_COLOR;
    DWORD  RemainingLength;
    DWORD  DestOffset;
    YORI_STRING SearchString;

    RemainingLength = BufferLength;

    TextString->LengthInChars = 0;
    DestOffset = 0;

    //
    //  We expect an escape initiator (two chars) and a 'm' for color
    //  formatting.  This should already be validated, the check here
    //  is redundant.
    //

    if (BufferLength >= 3 &&
        SrcPoint[BufferLength - 1] == 'm') {

        DWORD code;
        TCHAR NewTag[128];
        BOOLEAN NewUnderline = FALSE;
        DWORD ColorTable[] = {0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xc0c0c0,
                              0x808080, 0xff0000, 0x00ff00, 0xffff00, 0x0000ff, 0xff00ff, 0x00ffff, 0xffffff};

        SrcPoint++;
        RemainingLength--;

        //
        //  For something manipulating colors, go through the semicolon
        //  delimited list and apply the changes to the current color
        //

        while (TRUE) {

            //
            //  Swallow either the ';' or '[' depending on whether we're
            //  at the start or an intermediate component
            //

            SrcPoint++;
            RemainingLength--;

            SearchString.StartOfString = SrcPoint;
            SearchString.LengthInChars = RemainingLength;
            code = YoriLibDecimalStringToInt(&SearchString);
            if (code == 0) {
                NewColor = CVTVT_DEFAULT_COLOR;
                NewUnderline = FALSE;
            } else if (code == 1) {
                NewColor |= 8;
            } else if (code == 4) {
                NewUnderline = TRUE;
            } else if (code == 7) {
                NewColor = (WORD)(((NewColor & 0xf) << 4) | ((NewColor & 0xf0) >> 4));
            } else if (code == 39) {
                NewColor = (WORD)((NewColor & ~(0xf)) | (CVTVT_DEFAULT_COLOR & 0xf));
            } else if (code == 49) {
                NewColor = (WORD)((NewColor & ~(0xf0)) | (CVTVT_DEFAULT_COLOR & 0xf0));
            } else if (code >= 30 && code <= 37) {
                NewColor = (WORD)((NewColor & ~(0xf)) | (code - 30));
            } else if (code >= 40 && code <= 47) {
                NewColor = (WORD)((NewColor & ~(0xf0)) | ((code - 40)<<4));
            } else if (code >= 90 && code <= 97) {
                NewColor = (WORD)((NewColor & ~(0xf)) | 0x8 | (code - 90));
            } else if (code >= 100 && code <= 107) {
                NewColor = (WORD)((NewColor & ~(0xf0)) | 0x80 | ((code - 100)<<4));
            }

            //
            //  We expect this to end with an 'm' at the end of the string, or
            //  some other delimiter in the middle.  For paranoia maintain
            //  length checking.
            //

            YoriLibInitEmptyString(&SearchString);
            SearchString.StartOfString = SrcPoint;
            SearchString.LengthInChars = RemainingLength;
            SrcOffset = YoriLibCountStringContainingChars(&SearchString, _T("0123456789"));

            SrcPoint += SrcOffset;
            RemainingLength -= SrcOffset;

            if (RemainingLength == 0 ||
                *SrcPoint != ';') {
                
                break;
            }
        }

        if (YoriLibHtmlTagOpen) {
            if (YoriLibHtmlUnderlineOn) {
                if (DestOffset + sizeof("</U>") - 1 < TextString->LengthAllocated) {
                    memcpy(&TextString->StartOfString[DestOffset], _T("</U>"), sizeof(_T("</U>")) - sizeof(TCHAR));
                }
                DestOffset += sizeof("</U>") - 1;
            }
            if (YoriLibHtmlVersion == 4) {
                if (DestOffset + sizeof("</FONT>") - 1 < TextString->LengthAllocated) {
                    memcpy(&TextString->StartOfString[DestOffset], _T("</FONT>"), sizeof(_T("</FONT>")) - sizeof(TCHAR));
                }
                DestOffset += sizeof("</FONT>") - 1;
            } else {
                if (DestOffset + sizeof("</SPAN>") - 1 < TextString->LengthAllocated) {
                    memcpy(&TextString->StartOfString[DestOffset], _T("</SPAN>"), sizeof(_T("</SPAN>")) - sizeof(TCHAR));
                }
                DestOffset += sizeof("</SPAN>") - 1;
            }
        }

        //
        //  Output the appropriate tag depending on the version the user wanted
        //

        if (YoriLibHtmlVersion == 4) {
            SrcOffset = YoriLibSPrintf(NewTag,
                                       _T("<FONT COLOR=#%06x>"),
                                       (int)ColorTable[NewColor & 0xf]);
        } else {
            SrcOffset = YoriLibSPrintf(NewTag,
                                       _T("<SPAN STYLE=\"color:#%06x;background-color:#%06x\">"),
                                      (int)ColorTable[NewColor & 0xf],
                                      (int)ColorTable[(NewColor & 0xf0) >> 4]);
        }

        if (DestOffset + SrcOffset < TextString->LengthAllocated) {
            memcpy(&TextString->StartOfString[DestOffset], NewTag, SrcOffset * sizeof(TCHAR));
        }
        DestOffset += SrcOffset;

        if (NewUnderline) {
            if (DestOffset + sizeof("<U>") - 1 < TextString->LengthAllocated) {
                memcpy(&TextString->StartOfString[DestOffset], _T("<U>"), sizeof(_T("<U>")) - sizeof(TCHAR));
            }
            DestOffset += sizeof("<U>") - 1;
        }

        if (TextString->StartOfString != NULL) {
            YoriLibHtmlUnderlineOn = NewUnderline;
            YoriLibHtmlTagOpen = TRUE;
        }
    }

    if (DestOffset < TextString->LengthAllocated) {
        TextString->StartOfString[DestOffset] = '\0';
        TextString->LengthInChars = DestOffset;
    }

    *BufferSizeNeeded = DestOffset + 1;

    return TRUE;
}

/**
 Set the version of HTML to use for future encoding operations.

 @param HtmlVersion The version to use.  Can be 4 or 5.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriLibHtmlSetVersion(
    __in DWORD HtmlVersion
    )
{
    if (HtmlVersion != 4 && HtmlVersion != 5) {
        return FALSE;
    }
    YoriLibHtmlVersion = HtmlVersion;
    return TRUE;
}

/**
 A context which can be passed around as a "handle" when generating an HTML
 output string from VT100 text.
 */
typedef struct _YORI_LIB_HTML_CONVERT_CONTEXT {

    /**
     Pointer to the Html buffer generated thus far.  This may be periodically
     reallocated.
     */
    PYORI_STRING HtmlText;
} YORI_LIB_HTML_CONVERT_CONTEXT, *PYORI_LIB_HTML_CONVERT_CONTEXT;

/**
 Append one Yori string to the tail of another, reallocating the combined
 buffer as required.

 @param StringToAppendTo The first string which should have the contents of
        the second string appended to it.  This string may be reallocated.

 @param StringToAdd The string to append to the first string.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriLibHtmlCvtAppendWithReallocate(
    __inout PYORI_STRING StringToAppendTo,
    __in PYORI_STRING StringToAdd
    )
{
    if (StringToAppendTo->LengthInChars + StringToAdd->LengthInChars > StringToAppendTo->LengthAllocated) {
        if (!YoriLibReallocateString(StringToAppendTo, (StringToAppendTo->LengthInChars + StringToAdd->LengthInChars) * 4)) {
            return FALSE;
        }
    }

    memcpy(&StringToAppendTo->StartOfString[StringToAppendTo->LengthInChars], StringToAdd->StartOfString, StringToAdd->LengthInChars * sizeof(TCHAR));
    StringToAppendTo->LengthInChars += StringToAdd->LengthInChars;

    return TRUE;
}

/**
 Indicate the beginning of a stream and perform any initial output.

 @param hOutput The context to output any footer information to.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriLibHtmlCnvInitializeStream(
    __in HANDLE hOutput
    )
{
    YORI_STRING OutputString;
    PYORI_LIB_HTML_CONVERT_CONTEXT Context = (PYORI_LIB_HTML_CONVERT_CONTEXT)hOutput;

    YoriLibInitEmptyString(&OutputString);
    if (!YoriLibHtmlGenerateInitialString(&OutputString)) {
        return FALSE;
    }

    if (!YoriLibHtmlCvtAppendWithReallocate(Context->HtmlText, &OutputString)) {
        YoriLibFreeStringContents(&OutputString);
        return FALSE;
    }

    YoriLibFreeStringContents(&OutputString);
    return TRUE;
}

/**
 Indicate the end of the stream has been reached and perform any final
 output.

 @param hOutput The context to output any footer information to.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriLibHtmlCnvEndStream(
    __in HANDLE hOutput
    )
{
    YORI_STRING OutputString;
    PYORI_LIB_HTML_CONVERT_CONTEXT Context = (PYORI_LIB_HTML_CONVERT_CONTEXT)hOutput;
    YoriLibInitEmptyString(&OutputString);

    if (!YoriLibHtmlGenerateEndString(&OutputString)) {
        return FALSE;
    }

    if (!YoriLibHtmlCvtAppendWithReallocate(Context->HtmlText, &OutputString)) {
        YoriLibFreeStringContents(&OutputString);
        return FALSE;
    }

    YoriLibFreeStringContents(&OutputString);
    return TRUE;
}

/**
 Parse text between VT100 escape sequences and generate correct output for
 either HTML4 or HTML5.

 @param hOutput The output context to populate with the translated text
        information.

 @param StringBuffer Pointer to a string buffer containing the text to
        process.

 @param BufferLength Indicates the number of characters in StringBuffer.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriLibHtmlCnvProcessAndOutputText(
    __in HANDLE hOutput,
    __in LPTSTR StringBuffer,
    __in DWORD BufferLength
    )
{

    YORI_STRING TextString;
    DWORD BufferSizeNeeded;
    PYORI_LIB_HTML_CONVERT_CONTEXT Context = (PYORI_LIB_HTML_CONVERT_CONTEXT)hOutput;

    YoriLibInitEmptyString(&TextString);
    BufferSizeNeeded = 0;
    if (!YoriLibHtmlGenerateTextString(&TextString, &BufferSizeNeeded, StringBuffer, BufferLength)) {
        return FALSE;
    }

    if (!YoriLibAllocateString(&TextString, BufferSizeNeeded)) {
        return FALSE;
    }

    BufferSizeNeeded = 0;
    if (!YoriLibHtmlGenerateTextString(&TextString, &BufferSizeNeeded, StringBuffer, BufferLength)) {
        YoriLibFreeStringContents(&TextString);
        return FALSE;
    }

    if (!YoriLibHtmlCvtAppendWithReallocate(Context->HtmlText, &TextString)) {
        YoriLibFreeStringContents(&TextString);
        return FALSE;
    }

    YoriLibFreeStringContents(&TextString);
    return TRUE;
}


/**
 Parse a VT100 escape sequence and generate the correct output for either
 HTML4 or HTML5.

 @param hOutput The output context to populate with the translated escape
        information.

 @param StringBuffer Pointer to a string buffer containing the escape to
        process.

 @param BufferLength Indicates the number of characters in StringBuffer.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriLibHtmlCnvProcessAndOutputEscape(
    __in HANDLE hOutput,
    __in LPTSTR StringBuffer,
    __in DWORD BufferLength
    )
{
    YORI_STRING TextString;
    DWORD BufferSizeNeeded;
    PYORI_LIB_HTML_CONVERT_CONTEXT Context = (PYORI_LIB_HTML_CONVERT_CONTEXT)hOutput;

    YoriLibInitEmptyString(&TextString);
    BufferSizeNeeded = 0;
    if (!YoriLibHtmlGenerateEscapeString(&TextString, &BufferSizeNeeded, StringBuffer, BufferLength)) {
        return FALSE;
    }

    if (!YoriLibAllocateString(&TextString, BufferSizeNeeded)) {
        return FALSE;
    }

    BufferSizeNeeded = 0;
    if (!YoriLibHtmlGenerateEscapeString(&TextString, &BufferSizeNeeded, StringBuffer, BufferLength)) {
        YoriLibFreeStringContents(&TextString);
        return FALSE;
    }

    if (!YoriLibHtmlCvtAppendWithReallocate(Context->HtmlText, &TextString)) {
        YoriLibFreeStringContents(&TextString);
        return FALSE;
    }

    YoriLibFreeStringContents(&TextString);
    return TRUE;
}


/**
 Convert a Yori string containing VT100 text into HTML with the specified
 format.

 @param VtText Pointer to the string to convert.

 @param HtmlText On successful completion, updated to point to an HTML
        representation.  This string will be reallocated within this routine.

 @param HtmlVersion Specifies the format of HTML to use.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriLibHtmlConvertToHtmlFromVt(
    __in PYORI_STRING VtText,
    __inout PYORI_STRING HtmlText,
    __in DWORD HtmlVersion
    )
{
    YORI_LIB_VT_CALLBACK_FUNCTIONS CallbackFunctions;
    YORI_LIB_HTML_CONVERT_CONTEXT Context;

    YoriLibHtmlSetVersion(HtmlVersion);

    Context.HtmlText = HtmlText;

    CallbackFunctions.InitializeStream = YoriLibHtmlCnvInitializeStream;
    CallbackFunctions.EndStream = YoriLibHtmlCnvEndStream;
    CallbackFunctions.ProcessAndOutputText = YoriLibHtmlCnvProcessAndOutputText;
    CallbackFunctions.ProcessAndOutputEscape = YoriLibHtmlCnvProcessAndOutputEscape;

    if (!YoriLibHtmlCnvInitializeStream((HANDLE)&Context)) {
        return FALSE;
    }

    if (!YoriLibProcessVtEscapesOnOpenStream(VtText->StartOfString,
                                             VtText->LengthInChars,
                                             (HANDLE)&Context,
                                             &CallbackFunctions)) {

        return FALSE;
    }

    if (!YoriLibHtmlCnvEndStream((HANDLE)&Context)) {
        return FALSE;
    }

    return TRUE;
}

// vim:sw=4:ts=4:et: