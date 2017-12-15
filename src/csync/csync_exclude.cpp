/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "config_csync.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "c_lib.h"
#include "c_private.h"
#include "c_utf8.h"

#include "csync_private.h"
#include "csync_exclude.h"
#include "csync_misc.h"

#include "common/utility.h"

#include <QString>
#include <QFileInfo>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#define CSYNC_LOG_CATEGORY_NAME "csync.exclude"
#include "csync_log.h"

/** Expands C-like escape sequences.
 *
 * The returned string is heap-allocated and owned by the caller.
 */
static const char *csync_exclude_expand_escapes(const char * input)
{
    size_t i_len = strlen(input) + 1;
    char *out = (char*)c_malloc(i_len); // out can only be shorter

    size_t i = 0;
    size_t o = 0;
    for (; i < i_len; ++i) {
        if (input[i] == '\\') {
            // at worst input[i+1] is \0
            switch (input[i+1]) {
            case '\'': out[o++] = '\''; break;
            case '"': out[o++] = '"'; break;
            case '?': out[o++] = '?'; break;
            case '#': out[o++] = '#'; break;
            case 'a': out[o++] = '\a'; break;
            case 'b': out[o++] = '\b'; break;
            case 'f': out[o++] = '\f'; break;
            case 'n': out[o++] = '\n'; break;
            case 'r': out[o++] = '\r'; break;
            case 't': out[o++] = '\t'; break;
            case 'v': out[o++] = '\v'; break;
            default:
                // '\*' '\?' '\[' '\\' will be processed during regex translation
                // '\\' is intentionally not expanded here (to avoid '\\*' and '\*'
                // ending up meaning the same thing)
                out[o++] = input[i];
                out[o++] = input[i+1];
                break;
            }
            ++i;
        } else {
            out[o++] = input[i];
        }
    }
    return out;
}

/** Loads patterns from a file and adds them to excludes */
int csync_exclude_load(const char *fname, QList<QByteArray> *excludes) {
  int fd = -1;
  int i = 0;
  int rc = -1;
  int64_t size;
  char *buf = NULL;
  char *entry = NULL;
  mbchar_t *w_fname;

  if (fname == NULL) {
      return -1;
  }

#ifdef _WIN32
  _fmode = _O_BINARY;
#endif

  w_fname = c_utf8_path_to_locale(fname);
  if (w_fname == NULL) {
      return -1;
  }

  fd = _topen(w_fname, O_RDONLY);
  c_free_locale_string(w_fname);
  if (fd < 0) {
    return -1;
  }

  size = lseek(fd, 0, SEEK_END);
  if (size < 0) {
    rc = -1;
    goto out;
  }
  lseek(fd, 0, SEEK_SET);
  if (size == 0) {
    rc = 0;
    goto out;
  }
  buf = (char*)c_malloc(size + 1);
  if (read(fd, buf, size) != size) {
    rc = -1;
    goto out;
  }
  buf[size] = '\0';

  /* FIXME: Use fgets and don't add duplicates */
  entry = buf;
  for (i = 0; i < size; i++) {
    if (buf[i] == '\n' || buf[i] == '\r') {
      if (entry != buf + i) {
        buf[i] = '\0';
        if (*entry != '#') {
          const char *unescaped = csync_exclude_expand_escapes(entry);
          excludes->append(unescaped);
          CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Adding entry: %s", unescaped);
          SAFE_FREE(unescaped);
        }
      }
      entry = buf + i + 1;
    }
  }

  rc = 0;
out:
  SAFE_FREE(buf);
  close(fd);
  return rc;
}

// See http://support.microsoft.com/kb/74496 and
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx
// Additionally, we ignore '$Recycle.Bin', see https://github.com/owncloud/client/issues/2955
static const char *win_reserved_words_3[] = { "CON", "PRN", "AUX", "NUL" };
static const char *win_reserved_words_4[] = {
    "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
    "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
};
static const char *win_reserved_words_n[] = { "CLOCK$", "$Recycle.Bin" };

/**
 * @brief Checks if filename is considered reserved by Windows
 * @param file_name filename
 * @return true if file is reserved, false otherwise
 */
bool csync_is_windows_reserved_word(const char *filename)
{
    size_t len_filename = strlen(filename);

    // Drive letters
    if (len_filename == 2 && filename[1] == ':') {
        if (filename[0] >= 'a' && filename[0] <= 'z') {
            return true;
        }
        if (filename[0] >= 'A' && filename[0] <= 'Z') {
            return true;
        }
    }

    if (len_filename == 3 || (len_filename > 3 && filename[3] == '.')) {
        for (const char *word : win_reserved_words_3) {
            if (c_strncasecmp(filename, word, 3) == 0) {
                return true;
            }
        }
    }

    if (len_filename == 4 || (len_filename > 4 && filename[4] == '.')) {
        for (const char *word : win_reserved_words_4) {
            if (c_strncasecmp(filename, word, 4) == 0) {
                return true;
            }
        }
    }

    for (const char *word : win_reserved_words_n) {
        size_t len_word = strlen(word);
        if (len_word == len_filename && c_strncasecmp(filename, word, len_word) == 0) {
            return true;
        }
    }

    return false;
}

static CSYNC_EXCLUDE_TYPE _csync_excluded_common(const char *path)
{
    const char *bname = NULL;
    size_t blen = 0;
    int rc = -1;
    CSYNC_EXCLUDE_TYPE match = CSYNC_NOT_EXCLUDED;

    /* split up the path */
    bname = strrchr(path, '/');
    if (bname) {
        bname += 1; // don't include the /
    } else {
        bname = path;
    }
    blen = strlen(bname);

    // 9 = strlen(".sync_.db")
    if (blen >= 9 && bname[0] == '.') {
        rc = csync_fnmatch("._sync_*.db*", bname, 0);
        if (rc == 0) {
            match = CSYNC_FILE_SILENTLY_EXCLUDED;
            goto out;
        }
        rc = csync_fnmatch(".sync_*.db*", bname, 0);
        if (rc == 0) {
            match = CSYNC_FILE_SILENTLY_EXCLUDED;
            goto out;
        }
        rc = csync_fnmatch(".csync_journal.db*", bname, 0);
        if (rc == 0) {
            match = CSYNC_FILE_SILENTLY_EXCLUDED;
            goto out;
        }
        rc = csync_fnmatch(".owncloudsync.log*", bname, 0);
        if (rc == 0) {
            match = CSYNC_FILE_SILENTLY_EXCLUDED;
            goto out;
        }
    }

    // check the strlen and ignore the file if its name is longer than 254 chars.
    // whenever changing this also check createDownloadTmpFileName
    if (blen > 254) {
        match = CSYNC_FILE_EXCLUDE_LONG_FILENAME;
        goto out;
    }

#ifdef _WIN32
    // Windows cannot sync files ending in spaces (#2176). It also cannot
    // distinguish files ending in '.' from files without an ending,
    // as '.' is a separator that is not stored internally, so let's
    // not allow to sync those to avoid file loss/ambiguities (#416)
    if (blen > 1) {
        if (bname[blen-1]== ' ') {
            match = CSYNC_FILE_EXCLUDE_TRAILING_SPACE;
            goto out;
        } else if (bname[blen-1]== '.' ) {
            match = CSYNC_FILE_EXCLUDE_INVALID_CHAR;
            goto out;
        }
    }

    if (csync_is_windows_reserved_word(bname)) {
      match = CSYNC_FILE_EXCLUDE_INVALID_CHAR;
      goto out;
    }

    // Filter out characters not allowed in a filename on windows
    for (const char *p = path; *p; p++) {
        switch (*p) {
        case '\\':
        case ':':
        case '?':
        case '*':
        case '"':
        case '>':
        case '<':
        case '|':
            match = CSYNC_FILE_EXCLUDE_INVALID_CHAR;
            goto out;
        default:
            break;
        }
    }
#endif

    /* We create a desktop.ini on Windows for the sidebar icon, make sure we don't sync them. */
    if (blen == 11) {
        rc = csync_fnmatch("Desktop.ini", bname, 0);
        if (rc == 0) {
            match = CSYNC_FILE_SILENTLY_EXCLUDED;
            goto out;
        }
    }

    if (!OCC::Utility::shouldUploadConflictFiles()) {
        if (OCC::Utility::isConflictFile(bname)) {
            match = CSYNC_FILE_EXCLUDE_CONFLICT;
            goto out;
        }
    }

  out:

    return match;
}


using namespace OCC;

ExcludedFiles::ExcludedFiles()
{
}

ExcludedFiles::~ExcludedFiles()
{
}

void ExcludedFiles::addExcludeFilePath(const QString &path)
{
    _excludeFiles.insert(path);
}

void ExcludedFiles::addManualExclude(const QByteArray &expr)
{
    _manualExcludes.append(expr);
    _allExcludes.append(expr);
    prepare();
}

void ExcludedFiles::clearManualExcludes()
{
    _manualExcludes.clear();
    reloadExcludeFiles();
}

bool ExcludedFiles::reloadExcludeFiles()
{
    _allExcludes.clear();
    bool success = true;
    foreach (const QString &file, _excludeFiles) {
        if (csync_exclude_load(file.toUtf8(), &_allExcludes) < 0)
            success = false;
    }
    _allExcludes.append(_manualExcludes);
    prepare();
    return success;
}

bool ExcludedFiles::isExcluded(
    const QString &filePath,
    const QString &basePath,
    bool excludeHidden) const
{
    if (!filePath.startsWith(basePath, Utility::fsCasePreserving() ? Qt::CaseInsensitive : Qt::CaseSensitive)) {
        // Mark paths we're not responsible for as excluded...
        return true;
    }

    if (excludeHidden) {
        QString path = filePath;
        // Check all path subcomponents, but to *not* check the base path:
        // We do want to be able to sync with a hidden folder as the target.
        while (path.size() > basePath.size()) {
            QFileInfo fi(path);
            if (fi.isHidden() || fi.fileName().startsWith(QLatin1Char('.'))) {
                return true;
            }

            // Get the parent path
            path = fi.absolutePath();
        }
    }

    QFileInfo fi(filePath);
    ItemType type = ItemTypeFile;
    if (fi.isDir()) {
        type = ItemTypeDirectory;
    }

    QString relativePath = filePath.mid(basePath.size());
    if (relativePath.endsWith(QLatin1Char('/'))) {
        relativePath.chop(1);
    }

    return fullPatternMatch(relativePath.toUtf8(), type) != CSYNC_NOT_EXCLUDED;
}

CSYNC_EXCLUDE_TYPE ExcludedFiles::traversalPatternMatch(const char *path, ItemType filetype) const
{
    auto match = _csync_excluded_common(path);
    if (match != CSYNC_NOT_EXCLUDED)
        return match;
    if (_allExcludes.isEmpty())
        return CSYNC_NOT_EXCLUDED;

    // Check the bname part of the path to see whether the full
    // regex should be run.

    const char *bname = strrchr(path, '/');
    if (bname) {
        bname += 1; // don't include the /
    } else {
        bname = path;
    }
    QString bnameStr = QString::fromUtf8(bname);

    QRegularExpressionMatch m;
    if (filetype == ItemTypeDirectory) {
        m = _bnameActivationRegexDir.match(bnameStr);
    } else {
        m = _bnameActivationRegexFile.match(bnameStr);
    }
    if (!m.hasMatch())
        return match;

    // Now run the full match

    QString pathStr = QString::fromUtf8(path);
    if (filetype == ItemTypeDirectory) {
        m = _fullRegexDir.match(pathStr);
    } else {
        m = _fullRegexFile.match(pathStr);
    }
    if (m.hasMatch()) {
        if (!m.captured(1).isEmpty()) {
            match = CSYNC_FILE_EXCLUDE_LIST;
        } else if (!m.captured(2).isEmpty()) {
            match = CSYNC_FILE_EXCLUDE_AND_REMOVE;
        }
    }
    return match;
}

CSYNC_EXCLUDE_TYPE ExcludedFiles::fullPatternMatch(const char *path, ItemType filetype) const
{
    auto match = _csync_excluded_common(path);
    if (match != CSYNC_NOT_EXCLUDED)
        return match;
    if (_allExcludes.isEmpty())
        return CSYNC_NOT_EXCLUDED;

    QString p = QString::fromUtf8(path);
    QRegularExpressionMatch m;
    if (filetype == ItemTypeDirectory) {
        m = _fullRegexDir.match(p);
    } else {
        m = _fullRegexFile.match(p);
    }
    if (m.hasMatch()) {
        if (!m.captured(1).isEmpty()) {
            return CSYNC_FILE_EXCLUDE_LIST;
        } else if (!m.captured(2).isEmpty()) {
            return CSYNC_FILE_EXCLUDE_AND_REMOVE;
        }
    }
    return CSYNC_NOT_EXCLUDED;
}

auto ExcludedFiles::csyncTraversalMatchFun() const
    -> std::function<CSYNC_EXCLUDE_TYPE(const char *path, ItemType filetype)>
{
    return [this](const char *path, ItemType filetype) { return this->traversalPatternMatch(path, filetype); };
}

static QString convertToRegexpSyntax(QString exclude)
{
    // Translate *, ?, [...] to their regex variants.
    // The escape sequences \*, \?, \[. \\ have a special meaning,
    // the other ones have already been expanded before
    // (like "\\n" being replaced by "\n").
    //
    // QString being UTF-16 makes unicode-correct escaping tricky.
    // If we escaped each UTF-16 code unit we'd end up splitting 4-byte
    // code points. To avoid problems we delegate as much work as possible to
    // QRegularExpression::escape(): It always receives as long a sequence
    // as code units as possible.
    QString regex;
    int i = 0;
    int charsToEscape = 0;
    auto flush = [&]() {
        regex.append(QRegularExpression::escape(exclude.mid(i - charsToEscape, charsToEscape)));
        charsToEscape = 0;
    };
    auto len = exclude.size();
    for (; i < len; ++i) {
        switch (exclude[i].unicode()) {
        case '*':
            flush();
            regex.append("[^/]*");
            break;
        case '?':
            flush();
            regex.append("[^/]");
            break;
        case '[': {
            flush();
            // Find the end of the bracket expression
            auto j = i + 1;
            for (; j < len; ++j) {
                if (exclude[j] == ']')
                    break;
                if (j != len - 1 && exclude[j] == '\\' && exclude[j + 1] == ']')
                    ++j;
            }
            if (j == len) {
                // no matching ], just insert the escaped [
                regex.append("\\[");
                break;
            }
            // Translate [! to [^
            QString bracketExpr = exclude.mid(i, j - i + 1);
            if (bracketExpr.startsWith("[!"))
                bracketExpr[1] = '^';
            regex.append(bracketExpr);
            i = j;
            break;
        }
        case '\\':
            flush();
            if (i == len - 1) {
                regex.append("\\\\");
                break;
            }
            // '\*' -> '\*', but '\z' -> '\\z'
            switch (exclude[i + 1].unicode()) {
            case '*':
            case '?':
            case '[':
            case '\\':
                regex.append(QRegularExpression::escape(exclude.mid(i + 1, 1)));
                break;
            default:
                charsToEscape += 2;
                break;
            }
            ++i;
            break;
        default:
            ++charsToEscape;
            break;
        }
    }
    flush();
    return regex;
}

void ExcludedFiles::prepare()
{
    // Build regular expressions for the different cases.
    //
    // To compose the _bnameActivationRegex and _fullRegex patterns we
    // collect several subgroups of patterns here.
    //
    // * The "full" group will contain all patterns that contain a non-trailing
    //   slash. They only make sense in the fullRegex.
    // * The "bname" group contains all patterns without a non-trailing slash.
    //   These need separate handling in the _fullRegex (slash-containing
    //   patterns must be anchored to the front, these don't need it)
    // * The "bnameTrigger" group contains the bname part of all patterns in the
    //   "full" group. These and the "bname" group become _bnameActivationRegex.
    //
    // To complicate matters, the exclude patterns have two binary attributes
    // meaning we'll end up with 4 variants:
    // * "]" patterns mean "EXCLUDE_AND_REMOVE", they get collected in the
    //   pattern strings ending in "Remove". The others go to "Keep".
    // * trailing-slash patterns match directories only. They get collected
    //   in the pattern strings saying "Dir", the others go into "FileDir"
    //   because they match files and directories.

    QString fullFileDirKeep;
    QString fullFileDirRemove;
    QString fullDirKeep;
    QString fullDirRemove;

    QString bnameFileDirKeep;
    QString bnameFileDirRemove;
    QString bnameDirKeep;
    QString bnameDirRemove;

    QString bnameTriggerFileDir;
    QString bnameTriggerDir;

    auto regexAppend = [](QString &fileDirPattern, QString &dirPattern, const QString &appendMe, bool dirOnly) {
        QString &pattern = dirOnly ? dirPattern : fileDirPattern;
        if (!pattern.isEmpty())
            pattern.append("|");
        pattern.append(appendMe);
    };

    for (auto exclude : _allExcludes) {
        if (exclude[0] == '\n')
            continue; // empty line
        if (exclude[0] == '\r')
            continue; // empty line

        bool matchDirOnly = exclude.endsWith('/');
        if (matchDirOnly)
            exclude = exclude.left(exclude.size() - 1);

        bool removeExcluded = (exclude[0] == ']');
        if (removeExcluded)
            exclude = exclude.mid(1);

        bool fullPath = exclude.contains('/');

        /* Use QRegularExpression, append to the right pattern */
        auto &bnameFileDir = removeExcluded ? bnameFileDirRemove : bnameFileDirKeep;
        auto &bnameDir = removeExcluded ? bnameDirRemove : bnameDirKeep;
        auto &fullFileDir = removeExcluded ? fullFileDirRemove : fullFileDirKeep;
        auto &fullDir = removeExcluded ? fullDirRemove : fullDirKeep;

        auto regexExclude = convertToRegexpSyntax(QString::fromUtf8(exclude));
        if (!fullPath) {
            regexAppend(bnameFileDir, bnameDir, regexExclude, matchDirOnly);
        } else {
            regexAppend(fullFileDir, fullDir, regexExclude, matchDirOnly);

            // for activation, trigger on the 'bname' part of the full pattern
            auto bnameExclude = exclude.mid(exclude.lastIndexOf('/') + 1);
            auto regexBname = convertToRegexpSyntax(bnameExclude);
            regexAppend(bnameTriggerFileDir, bnameTriggerDir, regexBname, matchDirOnly);
        }
    }

    // The empty pattern would match everything - change it to match-nothing
    auto emptyMatchNothing = [](QString &pattern) {
        if (pattern.isEmpty())
            pattern = "a^";
    };
    emptyMatchNothing(fullFileDirKeep);
    emptyMatchNothing(fullFileDirRemove);
    emptyMatchNothing(fullDirKeep);
    emptyMatchNothing(fullDirRemove);

    emptyMatchNothing(bnameFileDirKeep);
    emptyMatchNothing(bnameFileDirRemove);
    emptyMatchNothing(bnameDirKeep);
    emptyMatchNothing(bnameDirRemove);

    emptyMatchNothing(bnameTriggerFileDir);
    emptyMatchNothing(bnameTriggerDir);

    // The bname activation regexe is applied to the bname only, so must be
    // anchored in the beginning and in the end. It has the explicit triggers
    // plus the bname-only patterns. Here we don't care about the remove/keep
    // distinction.
    _bnameActivationRegexFile.setPattern(
        "^(?:" + bnameFileDirKeep + "|" + bnameFileDirRemove + "|" + bnameTriggerFileDir + ")$");
    _bnameActivationRegexDir.setPattern(
        "^(?:" + bnameFileDirKeep + "|" + bnameFileDirRemove
        + "|" + bnameDirKeep + "|" + bnameFileDirRemove
        + "|" + bnameTriggerFileDir + "|" + bnameTriggerDir + ")$");

    // The full regex has two captures, it's basic form is "(...)|(...)". The first
    // capture has the keep/exclude-only patterns, the second the remove/exclude-and-remove
    // patterns.
    _fullRegexFile.setPattern(
        QLatin1String("(")
        // Full patterns are anchored to the beginning
        + "^(?:" + fullFileDirKeep + ")(?:$|/)" + "|"
        // Simple bname patterns can be any path component
        + "(?:^|/)(?:" + bnameFileDirKeep + ")(?:$|/)" + "|"
        // When checking a file for exclusion we must check all parent paths
        // against the dir-only patterns as well.
        + "(?:^|/)(?:" + bnameDirKeep + ")/"
        + ")|("
        + "^(?:" + fullFileDirRemove + ")(?:$|/)" + "|"
        + "(?:^|/)(?:" + bnameFileDirRemove + ")(?:$|/)" + "|"
        + "(?:^|/)(?:" + bnameDirRemove + ")/"
        + ")");
    _fullRegexDir.setPattern(
        QLatin1String("(")
        + "^(?:" + fullFileDirKeep + "|" + fullDirKeep + ")(?:$|/)" + "|"
        + "(?:^|/)(?:" + bnameFileDirKeep + "|" + bnameDirKeep + ")(?:$|/)"
        + ")|("
        + "^(?:" + fullFileDirRemove + "|" + fullDirRemove + ")(?:$|/)" + "|"
        + "(?:^|/)(?:" + bnameFileDirRemove + "|" + bnameDirRemove + ")(?:$|/)"
        + ")");

    QRegularExpression::PatternOptions patternOptions = QRegularExpression::NoPatternOption;
    if (OCC::Utility::fsCasePreserving())
        patternOptions |= QRegularExpression::CaseInsensitiveOption;
    _bnameActivationRegexFile.setPatternOptions(patternOptions);
    _bnameActivationRegexFile.optimize();
    _bnameActivationRegexDir.setPatternOptions(patternOptions);
    _bnameActivationRegexDir.optimize();
    _fullRegexFile.setPatternOptions(patternOptions);
    _fullRegexFile.optimize();
    _fullRegexDir.setPatternOptions(patternOptions);
    _fullRegexDir.optimize();
}
