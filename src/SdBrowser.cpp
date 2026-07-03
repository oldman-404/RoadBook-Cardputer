#include "SdBrowser.h"

#include "StorageManager.h"
#include "Theme.h"

#include <SD.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kRootPath = "/roadbook";
constexpr const char* kJourneysPath = "/roadbook/journeys";
constexpr const char* kTiles100Path = "/roadbook/tiles/100";

constexpr uint16_t kMaxCountScanFiles = 512;

bool isDotEntry(const char* name)
{
    if (name == nullptr) {
        return true;
    }
    return std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0;
}

}  // namespace

void SdBrowser::begin(StorageManager& storage)
{
    storage_ = &storage;
    storageReady_ = storage.ready();
    view_ = storageReady_ ? SdBrowserView::Directory : SdBrowserView::Unavailable;
    previewMode_ = false;
    pathValid_ = true;
    std::snprintf(currentPath_, sizeof(currentPath_), "%s", kRootPath);
    Theme::formatDisplayPath(currentPath_, displayPath_, sizeof(displayPath_));
    currentPage_ = 0;
    totalPages_ = 1;
    selectedIndex_ = 0;
    entryCount_ = 0;
    setDirectoryState(SdBrowserDirectoryState::Ok);
    needsRefresh_ = true;
}

void SdBrowser::setStorageReady(bool ready)
{
    storageReady_ = ready;
    if (!storageReady_) {
        view_ = SdBrowserView::Unavailable;
        previewMode_ = false;
    } else if (view_ == SdBrowserView::Unavailable) {
        view_ = SdBrowserView::Directory;
        previewMode_ = false;
        std::snprintf(currentPath_, sizeof(currentPath_), "%s", kRootPath);
        Theme::formatDisplayPath(currentPath_, displayPath_, sizeof(displayPath_));
        needsRefresh_ = true;
    }
}

void SdBrowser::setDirectoryState(SdBrowserDirectoryState state)
{
    directoryState_ = state;
    switch (state) {
        case SdBrowserDirectoryState::Empty:
            std::snprintf(directoryStatusText_, sizeof(directoryStatusText_), "EMPTY FOLDER");
            break;
        case SdBrowserDirectoryState::OpenError:
            std::snprintf(directoryStatusText_, sizeof(directoryStatusText_), "OPEN ERROR");
            break;
        case SdBrowserDirectoryState::PathTooLong:
            std::snprintf(directoryStatusText_, sizeof(directoryStatusText_), "PATH TOO LONG");
            break;
        case SdBrowserDirectoryState::Ok:
        default:
            directoryStatusText_[0] = '\0';
            break;
    }
}

void SdBrowser::refresh()
{
    if (!storageReady_) {
        view_ = SdBrowserView::Unavailable;
        previewMode_ = false;
        return;
    }

    if (view_ == SdBrowserView::Directory) {
        loadDirectoryPage();
    } else if (view_ == SdBrowserView::TextPreview) {
        loadPreviewPage();
    } else if (view_ == SdBrowserView::SdInfo) {
        showSdInfo();
    }

    needsRefresh_ = false;
}

bool SdBrowser::endsWithIgnoreCase(const char* text, const char* suffix)
{
    if (text == nullptr || suffix == nullptr) {
        return false;
    }

    const size_t textLen = std::strlen(text);
    const size_t suffixLen = std::strlen(suffix);
    if (suffixLen == 0 || textLen < suffixLen) {
        return false;
    }

    const char* textSuffix = text + (textLen - suffixLen);
    for (size_t index = 0; index < suffixLen; ++index) {
        char left = textSuffix[index];
        char right = suffix[index];
        if (left >= 'A' && left <= 'Z') {
            left = static_cast<char>(left - 'A' + 'a');
        }
        if (right >= 'A' && right <= 'Z') {
            right = static_cast<char>(right - 'A' + 'a');
        }
        if (left != right) {
            return false;
        }
    }

    return true;
}

void SdBrowser::truncateLine(const char* source, char* dest, size_t destSize)
{
    if (destSize == 0) {
        return;
    }

    std::snprintf(dest, destSize, "%.*s", static_cast<int>(destSize - 1), source);
}

bool SdBrowser::hasValidSelection() const
{
    return entryCount_ > 0 && selectedIndex_ < entryCount_;
}

bool SdBrowser::countDirectoryEntries(uint16_t& totalEntries)
{
    totalEntries = 0;

    File dir = SD.open(currentPath_);
    if (!dir || !dir.isDirectory()) {
        if (dir) {
            dir.close();
        }
        return false;
    }

    for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
        const char* name = entry.name();
        if (name != nullptr && !isDotEntry(name)) {
            ++totalEntries;
        }
        entry.close();
        if (totalEntries >= 4096) {
            break;
        }
    }

    dir.close();
    return true;
}

bool SdBrowser::skipDirectoryEntries(File& dir, uint16_t skipCount)
{
    uint16_t skipped = 0;
    while (skipped < skipCount) {
        File entry = dir.openNextFile();
        if (!entry) {
            return false;
        }
        const char* name = entry.name();
        if (name != nullptr && !isDotEntry(name)) {
            ++skipped;
        }
        entry.close();
    }
    return true;
}

bool SdBrowser::loadDirectoryPage()
{
    entryCount_ = 0;
    selectedIndex_ = 0;
    pathValid_ = true;
    previewMode_ = false;
    Theme::formatDisplayPath(currentPath_, displayPath_, sizeof(displayPath_));

    uint16_t totalEntries = 0;
    if (!countDirectoryEntries(totalEntries)) {
        totalPages_ = 1;
        currentPage_ = 0;
        setDirectoryState(SdBrowserDirectoryState::OpenError);
        Serial.printf("SD BROWSER open failed path=%s\n", currentPath_);
        return false;
    }

    if (totalEntries == 0) {
        totalPages_ = 1;
        currentPage_ = 0;
        setDirectoryState(SdBrowserDirectoryState::Empty);
        Serial.printf("SD BROWSER empty path=%s\n", currentPath_);
        return true;
    }

    setDirectoryState(SdBrowserDirectoryState::Ok);

    const uint8_t pageSize = visibleEntriesPerPage();
    totalPages_ = static_cast<uint16_t>((totalEntries + pageSize - 1) / pageSize);
    if (totalPages_ == 0) {
        totalPages_ = 1;
    }
    if (currentPage_ >= totalPages_) {
        currentPage_ = totalPages_ - 1;
    }

    File dir = SD.open(currentPath_);
    if (!dir || !dir.isDirectory()) {
        if (dir) {
            dir.close();
        }
        setDirectoryState(SdBrowserDirectoryState::OpenError);
        Serial.printf("SD BROWSER open failed path=%s\n", currentPath_);
        return false;
    }

    const uint16_t skipCount = static_cast<uint16_t>(currentPage_ * pageSize);
    if (skipCount > 0 && !skipDirectoryEntries(dir, skipCount)) {
        dir.close();
        currentPage_ = 0;
        return loadDirectoryPage();
    }

    for (File entry = dir.openNextFile(); entry && entryCount_ < pageSize; entry = dir.openNextFile()) {
        const char* name = entry.name();
        if (name == nullptr || isDotEntry(name)) {
            entry.close();
            continue;
        }

        SdBrowserEntry& item = entries_[entryCount_];
        std::snprintf(item.name, sizeof(item.name), "%s", name);
        item.isDirectory = entry.isDirectory();
        item.sizeBytes = entry.isDirectory() ? 0 : entry.size();
        entry.close();
        ++entryCount_;
    }

    dir.close();
    selectedIndex_ = 0;
    return true;
}

bool SdBrowser::buildChildPath(const char* name, char* outPath, size_t outSize) const
{
    if (name == nullptr || name[0] == '\0') {
        return false;
    }

    int written = 0;
    if (std::strcmp(currentPath_, kRootPath) == 0) {
        written = std::snprintf(outPath, outSize, "%s/%s", kRootPath, name);
    } else {
        written = std::snprintf(outPath, outSize, "%s/%s", currentPath_, name);
    }

    if (written < 0 || static_cast<size_t>(written) >= outSize) {
        return false;
    }
    return true;
}

bool SdBrowser::buildParentPath(char* outPath, size_t outSize) const
{
    if (std::strcmp(currentPath_, kRootPath) == 0) {
        return false;
    }

    std::snprintf(outPath, outSize, "%s", currentPath_);
    char* slash = std::strrchr(outPath, '/');
    if (slash == nullptr || slash == outPath) {
        std::snprintf(outPath, outSize, "%s", kRootPath);
        return true;
    }

    *slash = '\0';
    if (outPath[0] == '\0') {
        std::snprintf(outPath, outSize, "%s", kRootPath);
    }
    return true;
}

bool SdBrowser::isPreviewSupported(const char* name) const
{
    return endsWithIgnoreCase(name, ".csv") || endsWithIgnoreCase(name, ".txt") ||
           endsWithIgnoreCase(name, ".log") || endsWithIgnoreCase(name, ".json") ||
           endsWithIgnoreCase(name, ".gpx");
}

void SdBrowser::moveSelectionUp()
{
    if (view_ != SdBrowserView::Directory || entryCount_ == 0) {
        return;
    }

    if (selectedIndex_ > 0) {
        --selectedIndex_;
    }
}

void SdBrowser::moveSelectionDown()
{
    if (view_ != SdBrowserView::Directory || entryCount_ == 0) {
        return;
    }

    if (selectedIndex_ + 1 < entryCount_) {
        ++selectedIndex_;
    }
}

void SdBrowser::nextPage()
{
    if (view_ == SdBrowserView::Directory) {
        if (totalPages_ > 0 && currentPage_ + 1 < totalPages_) {
            ++currentPage_;
            loadDirectoryPage();
        }
        return;
    }

    if (view_ == SdBrowserView::TextPreview) {
        if (previewPage_ + 1 < previewTotalPages_) {
            ++previewPage_;
            loadPreviewPage();
        }
    }
}

void SdBrowser::previousPage()
{
    if (view_ == SdBrowserView::Directory) {
        if (currentPage_ > 0) {
            --currentPage_;
            loadDirectoryPage();
        }
        return;
    }

    if (view_ == SdBrowserView::TextPreview) {
        if (previewPage_ > 0) {
            --previewPage_;
            loadPreviewPage();
        }
    }
}

bool SdBrowser::openFileDetail()
{
    if (!hasValidSelection()) {
        Serial.printf("SD BROWSER invalid selection index=%u count=%u\n", selectedIndex_, entryCount_);
        return false;
    }

    const SdBrowserEntry& item = entries_[selectedIndex_];
    if (item.isDirectory) {
        return false;
    }

    view_ = SdBrowserView::FileDetail;
    previewMode_ = false;
    return true;
}

bool SdBrowser::openTextPreview()
{
    if (!hasValidSelection()) {
        Serial.printf("SD BROWSER invalid selection index=%u count=%u\n", selectedIndex_, entryCount_);
        return false;
    }

    const SdBrowserEntry& item = entries_[selectedIndex_];
    if (item.isDirectory || !isPreviewSupported(item.name)) {
        view_ = SdBrowserView::FileDetail;
        previewMode_ = false;
        return false;
    }

    if (!buildChildPath(item.name, previewFilePath_, sizeof(previewFilePath_))) {
        setDirectoryState(SdBrowserDirectoryState::PathTooLong);
        pathValid_ = false;
        return false;
    }

    std::snprintf(previewDisplayName_, sizeof(previewDisplayName_), "%s", item.name);

    File file = SD.open(previewFilePath_, FILE_READ);
    if (!file) {
        Serial.printf("SD BROWSER open failed path=%s\n", previewFilePath_);
        view_ = SdBrowserView::FileDetail;
        previewMode_ = false;
        return false;
    }

    previewFileSize_ = file.size();
    file.close();

    Serial.printf("SD BROWSER open path=%s size=%llu\n", previewFilePath_,
                  static_cast<unsigned long long>(previewFileSize_));

    previewPage_ = 0;
    previewOffset_ = 0;
    countPreviewPages();
    view_ = SdBrowserView::TextPreview;
    previewMode_ = true;
    return loadPreviewPage();
}

void SdBrowser::countPreviewPages()
{
    if (previewFileSize_ == 0) {
        previewTotalPages_ = 1;
        return;
    }

    previewTotalPages_ = static_cast<uint16_t>((previewFileSize_ + kPreviewBufferBytes - 1) / kPreviewBufferBytes);
    if (previewTotalPages_ == 0) {
        previewTotalPages_ = 1;
    }
}

bool SdBrowser::loadPreviewPage()
{
    previewLineCount_ = 0;
    previewBuffer_[0] = '\0';

    File file = SD.open(previewFilePath_, FILE_READ);
    if (!file) {
        Serial.printf("SD BROWSER open failed path=%s\n", previewFilePath_);
        return false;
    }

    previewOffset_ = static_cast<size_t>(previewPage_) * kPreviewBufferBytes;
    if (previewOffset_ >= previewFileSize_) {
        file.close();
        return false;
    }

    if (!file.seek(previewOffset_)) {
        file.close();
        return false;
    }

    const size_t bytesToRead = kPreviewBufferBytes - 1;
    const size_t bytesRead = file.read(reinterpret_cast<uint8_t*>(previewBuffer_), bytesToRead);
    file.close();

    previewBuffer_[bytesRead] = '\0';
    splitPreviewLines();
    return true;
}

void SdBrowser::splitPreviewLines()
{
    previewLineCount_ = 0;
    size_t index = 0;

    while (previewBuffer_[index] != '\0' && previewLineCount_ < kPreviewMaxLines) {
        const size_t lineStart = index;
        while (previewBuffer_[index] != '\0' && previewBuffer_[index] != '\n' && previewBuffer_[index] != '\r') {
            ++index;
        }

        char line[kPreviewMaxLineChars + 1];
        const size_t lineLength = index - lineStart;
        if (lineLength >= sizeof(line)) {
            std::memcpy(line, previewBuffer_ + lineStart, sizeof(line) - 1);
            line[sizeof(line) - 1] = '\0';
        } else {
            std::memcpy(line, previewBuffer_ + lineStart, lineLength);
            line[lineLength] = '\0';
        }
        truncateLine(line, previewLines_[previewLineCount_], sizeof(previewLines_[previewLineCount_]));
        ++previewLineCount_;

        if (previewBuffer_[index] == '\r') {
            ++index;
        }
        if (previewBuffer_[index] == '\n') {
            ++index;
        }
    }
}

void SdBrowser::enterSelection()
{
    if (!storageReady_) {
        return;
    }

    if (view_ == SdBrowserView::FileDetail) {
        if (hasValidSelection() && isPreviewSupported(entries_[selectedIndex_].name)) {
            openTextPreview();
        }
        return;
    }

    if (view_ != SdBrowserView::Directory) {
        return;
    }

    if (entryCount_ == 0) {
        Serial.printf("SD BROWSER empty path=%s\n", currentPath_);
        return;
    }

    if (!hasValidSelection()) {
        Serial.printf("SD BROWSER invalid selection index=%u count=%u\n", selectedIndex_, entryCount_);
        return;
    }

    const SdBrowserEntry& item = entries_[selectedIndex_];
    if (item.name[0] == '\0') {
        return;
    }

    if (item.isDirectory) {
        char nextPath[192];
        if (!buildChildPath(item.name, nextPath, sizeof(nextPath))) {
            setDirectoryState(SdBrowserDirectoryState::PathTooLong);
            pathValid_ = false;
            Serial.printf("SD BROWSER open failed path=%s\n", currentPath_);
            return;
        }

        std::snprintf(currentPath_, sizeof(currentPath_), "%s", nextPath);
        currentPage_ = 0;
        selectedIndex_ = 0;
        loadDirectoryPage();
        return;
    }

    if (isPreviewSupported(item.name)) {
        openTextPreview();
    } else {
        openFileDetail();
    }
}

void SdBrowser::goParent()
{
    if (!storageReady_) {
        return;
    }

    if (view_ == SdBrowserView::TextPreview || view_ == SdBrowserView::FileDetail) {
        view_ = SdBrowserView::Directory;
        previewMode_ = false;
        return;
    }

    char parentPath[192];
    if (!buildParentPath(parentPath, sizeof(parentPath))) {
        return;
    }

    std::snprintf(currentPath_, sizeof(currentPath_), "%s", parentPath);
    currentPage_ = 0;
    selectedIndex_ = 0;
    loadDirectoryPage();
}

void SdBrowser::backFromPreview()
{
    if (view_ == SdBrowserView::TextPreview) {
        view_ = SdBrowserView::Directory;
        previewMode_ = false;
    }
}

uint32_t SdBrowser::countFilesInDirectory(const char* path, bool countOnlyBin) const
{
    uint32_t count = 0;
    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) {
        if (dir) {
            dir.close();
        }
        return 0;
    }

    for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
        if (entry.isDirectory()) {
            entry.close();
            continue;
        }

        const char* name = entry.name();
        if (countOnlyBin) {
            if (name != nullptr && endsWithIgnoreCase(name, ".bin")) {
                ++count;
            }
        } else if (name != nullptr) {
            ++count;
        }

        entry.close();
        if (count >= kMaxCountScanFiles) {
            break;
        }
    }

    dir.close();
    return count;
}

void SdBrowser::showSdInfo()
{
    if (!storageReady_ || storage_ == nullptr) {
        view_ = SdBrowserView::Unavailable;
        previewMode_ = false;
        return;
    }

    sdCardSizeBytes_ = SD.cardSize();
    sdTotalBytes_ = SD.totalBytes();
    sdUsedBytes_ = SD.usedBytes();
    journeyFileCount_ = countFilesInDirectory(kJourneysPath, false);
    tileBucketFileCount_ = countFilesInDirectory(kTiles100Path, true);
    view_ = SdBrowserView::SdInfo;
    previewMode_ = false;
}

SdBrowserView SdBrowser::view() const
{
    return view_;
}

bool SdBrowser::previewMode() const
{
    return previewMode_;
}

bool SdBrowser::storageReady() const
{
    return storageReady_;
}

bool SdBrowser::pathValid() const
{
    return pathValid_;
}

bool SdBrowser::isEmpty() const
{
    return directoryState_ == SdBrowserDirectoryState::Empty;
}

SdBrowserDirectoryState SdBrowser::directoryState() const
{
    return directoryState_;
}

const char* SdBrowser::directoryStatusText() const
{
    if (directoryStatusText_[0] == '\0') {
        return nullptr;
    }
    return directoryStatusText_;
}

const char* SdBrowser::currentPath() const
{
    return currentPath_;
}

const char* SdBrowser::displayPath() const
{
    return displayPath_;
}

uint16_t SdBrowser::currentPage() const
{
    if (view_ == SdBrowserView::TextPreview) {
        return previewPage_;
    }
    return currentPage_;
}

uint16_t SdBrowser::totalPages() const
{
    if (view_ == SdBrowserView::TextPreview) {
        return previewTotalPages_;
    }
    return totalPages_;
}

uint8_t SdBrowser::selectedIndex() const
{
    return selectedIndex_;
}

uint8_t SdBrowser::entryCount() const
{
    return entryCount_;
}

uint8_t SdBrowser::visibleEntriesPerPage() const
{
    return static_cast<uint8_t>(kMaxDisplayedEntries);
}

bool SdBrowser::getEntry(uint8_t index, SdBrowserEntry& out) const
{
    if (index >= entryCount_) {
        return false;
    }
    out = entries_[index];
    return true;
}

const char* SdBrowser::selectedName() const
{
    if (!hasValidSelection()) {
        return "--";
    }
    return entries_[selectedIndex_].name;
}

uint64_t SdBrowser::selectedSizeBytes() const
{
    if (!hasValidSelection()) {
        return 0;
    }
    return entries_[selectedIndex_].sizeBytes;
}

bool SdBrowser::selectedIsDirectory() const
{
    if (!hasValidSelection()) {
        return false;
    }
    return entries_[selectedIndex_].isDirectory;
}

const char* SdBrowser::selectedTypeLabel() const
{
    if (!hasValidSelection()) {
        return "--";
    }

    const char* name = entries_[selectedIndex_].name;
    if (entries_[selectedIndex_].isDirectory) {
        return "DIR";
    }
    if (endsWithIgnoreCase(name, ".csv")) {
        return "CSV";
    }
    if (endsWithIgnoreCase(name, ".txt")) {
        return "TXT";
    }
    if (endsWithIgnoreCase(name, ".log")) {
        return "LOG";
    }
    if (endsWithIgnoreCase(name, ".json")) {
        return "JSON";
    }
    if (endsWithIgnoreCase(name, ".gpx")) {
        return "GPX";
    }
    if (endsWithIgnoreCase(name, ".bin")) {
        return "BIN";
    }
    return "FILE";
}

uint16_t SdBrowser::previewPage() const
{
    return previewPage_;
}

uint16_t SdBrowser::previewTotalPages() const
{
    return previewTotalPages_;
}

uint8_t SdBrowser::previewLineCount() const
{
    return previewLineCount_;
}

const char* SdBrowser::previewLine(uint8_t index) const
{
    if (index >= previewLineCount_) {
        return "";
    }
    return previewLines_[index];
}

const char* SdBrowser::previewDisplayName() const
{
    if (previewDisplayName_[0] == '\0') {
        return "--";
    }
    return previewDisplayName_;
}

uint64_t SdBrowser::sdCardSizeBytes() const
{
    return sdCardSizeBytes_;
}

uint64_t SdBrowser::sdTotalBytes() const
{
    return sdTotalBytes_;
}

uint64_t SdBrowser::sdUsedBytes() const
{
    return sdUsedBytes_;
}

const char* SdBrowser::selfTestLabel() const
{
    if (storage_ == nullptr) {
        return "NOT_RUN";
    }
    return storage_->selfTestResultText();
}

uint32_t SdBrowser::journeyFileCount() const
{
    return journeyFileCount_;
}

uint32_t SdBrowser::tileBucketFileCount() const
{
    return tileBucketFileCount_;
}

bool SdBrowser::needsRefresh() const
{
    return needsRefresh_;
}

void SdBrowser::clearNeedsRefresh()
{
    needsRefresh_ = false;
}

void SdBrowser::requestRefresh()
{
    needsRefresh_ = true;
}
