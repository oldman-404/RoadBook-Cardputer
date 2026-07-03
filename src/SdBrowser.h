#pragma once

#include <FS.h>

#include <cstddef>
#include <cstdint>

class StorageManager;

enum class SdBrowserView {
    Unavailable,
    Directory,
    FileDetail,
    TextPreview,
    SdInfo,
};

enum class SdBrowserDirectoryState {
    Ok,
    Empty,
    OpenError,
    PathTooLong,
};

struct SdBrowserEntry {
    char name[64];
    bool isDirectory;
    uint64_t sizeBytes;
};

class SdBrowser {
public:
    static constexpr size_t kMaxDisplayedEntries = 6;
    static constexpr size_t kMaxPathLength = 191;
    static constexpr size_t kPreviewBufferBytes = 1024;
    static constexpr size_t kPreviewMaxLines = 8;
    static constexpr size_t kPreviewMaxLineChars = 38;

    void begin(StorageManager& storage);
    void setStorageReady(bool ready);

    void refresh();
    void moveSelectionUp();
    void moveSelectionDown();
    void nextPage();
    void previousPage();
    void enterSelection();
    void goParent();
    void backFromPreview();
    void showSdInfo();
    void requestRefresh();

    static bool endsWithIgnoreCase(const char* text, const char* suffix);

    SdBrowserView view() const;
    bool previewMode() const;
    bool storageReady() const;
    bool pathValid() const;
    bool isEmpty() const;
    SdBrowserDirectoryState directoryState() const;
    const char* directoryStatusText() const;
    const char* currentPath() const;
    const char* displayPath() const;
    uint16_t currentPage() const;
    uint16_t totalPages() const;
    uint8_t selectedIndex() const;
    uint8_t entryCount() const;
    uint8_t visibleEntriesPerPage() const;
    bool getEntry(uint8_t index, SdBrowserEntry& out) const;

    const char* selectedName() const;
    uint64_t selectedSizeBytes() const;
    bool selectedIsDirectory() const;
    const char* selectedTypeLabel() const;

    uint16_t previewPage() const;
    uint16_t previewTotalPages() const;
    uint8_t previewLineCount() const;
    const char* previewLine(uint8_t index) const;
    const char* previewDisplayName() const;

    uint64_t sdCardSizeBytes() const;
    uint64_t sdTotalBytes() const;
    uint64_t sdUsedBytes() const;
    const char* selfTestLabel() const;
    uint32_t journeyFileCount() const;
    uint32_t tileBucketFileCount() const;

    bool needsRefresh() const;
    void clearNeedsRefresh();

private:
    bool hasValidSelection() const;
    bool countDirectoryEntries(uint16_t& totalEntries);
    bool skipDirectoryEntries(File& dir, uint16_t skipCount);
    bool loadDirectoryPage();
    bool buildChildPath(const char* name, char* outPath, size_t outSize) const;
    bool buildParentPath(char* outPath, size_t outSize) const;
    bool isPreviewSupported(const char* name) const;
    bool openFileDetail();
    bool openTextPreview();
    bool loadPreviewPage();
    void splitPreviewLines();
    void countPreviewPages();
    void setDirectoryState(SdBrowserDirectoryState state);
    uint32_t countFilesInDirectory(const char* path, bool countOnlyBin) const;

    static void truncateLine(const char* source, char* dest, size_t destSize);

    StorageManager* storage_{nullptr};
    bool storageReady_{false};
    bool needsRefresh_{true};
    bool pathValid_{true};
    bool previewMode_{false};
    SdBrowserView view_{SdBrowserView::Unavailable};
    SdBrowserDirectoryState directoryState_{SdBrowserDirectoryState::Ok};

    char currentPath_[192];
    char displayPath_[192];
    char directoryStatusText_[20];
    SdBrowserEntry entries_[6];
    uint8_t entryCount_{0};
    uint8_t selectedIndex_{0};
    uint16_t currentPage_{0};
    uint16_t totalPages_{1};

    char previewFilePath_[192];
    char previewDisplayName_[64];
    char previewBuffer_[1024];
    char previewLines_[8][40];
    uint8_t previewLineCount_{0};
    uint16_t previewPage_{0};
    uint16_t previewTotalPages_{1};
    uint64_t previewFileSize_{0};
    size_t previewOffset_{0};

    uint64_t sdCardSizeBytes_{0};
    uint64_t sdTotalBytes_{0};
    uint64_t sdUsedBytes_{0};
    uint32_t journeyFileCount_{0};
    uint32_t tileBucketFileCount_{0};
};
