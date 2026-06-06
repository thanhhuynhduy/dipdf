#ifndef PAGECACHE_H
#define PAGECACHE_H

#include <QPixmap>
#include <QHash>
#include <QLoggingCategory>
#include <list>

Q_DECLARE_LOGGING_CATEGORY(lcPageCache)

// ────────────────────────────────────────────────────────────────────────────
// Cache key — uniquely identifies a rendered page at a specific zoom / DPR.
// Zoom and DPR are stored as integers (value × 1000) to avoid floating-point
// comparison issues in hash lookups.
// ────────────────────────────────────────────────────────────────────────────
struct PageCacheKey {
    quintptr docId;      // Pointer-based document identifier
    int      pageIndex;
    int      zoomPermille;  // zoom × 1000
    int      dprPermille;   // devicePixelRatio × 1000

    bool operator==(const PageCacheKey &o) const {
        return docId == o.docId
            && pageIndex == o.pageIndex
            && zoomPermille == o.zoomPermille
            && dprPermille == o.dprPermille;
    }
};

inline size_t qHash(const PageCacheKey &k, size_t seed = 0) {
    // Combine hashes of all fields for a good distribution.
    size_t h = qHash(k.docId, seed);
    h ^= qHash(k.pageIndex, seed) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= qHash(k.zoomPermille, seed) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= qHash(k.dprPermille, seed) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
}

// Helper to build a cache key from floating-point values.
inline PageCacheKey makeCacheKey(quintptr docId, int pageIndex, double zoom, double dpr) {
    return { docId, pageIndex, static_cast<int>(zoom * 1000.0 + 0.5),
                               static_cast<int>(dpr * 1000.0 + 0.5) };
}

// ────────────────────────────────────────────────────────────────────────────
// PageCache — A memory-bounded LRU cache for QPixmap entries.
//
// * insert() adds a pixmap and evicts the least-recently-used entries when
//   the total byte usage exceeds the configured budget.
// * get() returns a cached pixmap (promoting it to MRU) or a null QPixmap.
// * clearForDocument() removes all entries belonging to a specific document.
// ────────────────────────────────────────────────────────────────────────────
class PageCache {
public:
    explicit PageCache(qint64 budgetBytes,
                       const QString &name = QStringLiteral("PageCache"));
    ~PageCache();

    // Retrieve a cached pixmap.  Returns null QPixmap on miss.
    QPixmap get(const PageCacheKey &key);

    // Insert a pixmap.  Evicts LRU entries if the budget would be exceeded.
    void insert(const PageCacheKey &key, const QPixmap &pixmap);

    // Remove all entries for a given document.
    void clearForDocument(quintptr docId);

    // Remove all entries for a document except those at the given zoom.
    void clearForDocumentExceptZoom(quintptr docId, int keepZoomPermille);

    // Remove everything.
    void clear();

    qint64 currentUsageBytes() const { return m_currentUsage; }
    qint64 budgetBytes()       const { return m_budget; }
    int    entryCount()        const { return static_cast<int>(m_entries.size()); }

    // Print a one-line summary via qCDebug(lcPageCache).
    void logStats() const;

private:
    void evictToFit(qint64 requiredSpace);
    static qint64 pixmapCost(const QPixmap &pm);

    QString m_name;
    qint64  m_budget;
    qint64  m_currentUsage = 0;

    // LRU list: front = most-recently-used, back = least-recently-used.
    using LruList = std::list<PageCacheKey>;
    LruList m_lruList;

    struct Entry {
        QPixmap           pixmap;
        qint64            cost;
        LruList::iterator lruIter;
    };

    QHash<PageCacheKey, Entry> m_entries;
};

#endif // PAGECACHE_H
