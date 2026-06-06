#include "pagecache.h"
#include <QDebug>

Q_LOGGING_CATEGORY(lcPageCache, "dipdf.cache", QtInfoMsg)

// ────────────────────────────────────────────────────────────────────────────

PageCache::PageCache(qint64 budgetBytes, const QString &name)
    : m_name(name), m_budget(budgetBytes)
{
    qCDebug(lcPageCache) << m_name << "created with budget"
                         << (m_budget / (1024 * 1024)) << "MB";
}

PageCache::~PageCache() {
    clear();
}

// ────────────────────────────────────────────────────────────────────────────
// Cost estimation — width × height × (depth / 8).
// For typical 32-bit ARGB pixmaps this gives 4 bytes per pixel.
// ────────────────────────────────────────────────────────────────────────────

qint64 PageCache::pixmapCost(const QPixmap &pm) {
    if (pm.isNull()) return 0;
    const qint64 bpp = qMax(pm.depth(), 1);
    return static_cast<qint64>(pm.width()) * pm.height() * (bpp / 8);
}

// ────────────────────────────────────────────────────────────────────────────
// get() — O(1) lookup + promote to MRU.
// ────────────────────────────────────────────────────────────────────────────

QPixmap PageCache::get(const PageCacheKey &key) {
    auto it = m_entries.find(key);
    if (it == m_entries.end()) {
        return QPixmap(); // cache miss
    }
    // Promote to front of LRU list (most recently used).
    m_lruList.splice(m_lruList.begin(), m_lruList, it->lruIter);
    return it->pixmap;
}

// ────────────────────────────────────────────────────────────────────────────
// insert() — Add or replace, then evict if over budget.
// ────────────────────────────────────────────────────────────────────────────

void PageCache::insert(const PageCacheKey &key, const QPixmap &pixmap) {
    const qint64 cost = pixmapCost(pixmap);

    // If the single entry is larger than the entire budget, don't cache it.
    if (cost > m_budget) {
        qCDebug(lcPageCache) << m_name << "entry too large for cache:"
                             << (cost / (1024 * 1024)) << "MB (budget"
                             << (m_budget / (1024 * 1024)) << "MB)";
        return;
    }

    // Remove existing entry if present (update case).
    auto existing = m_entries.find(key);
    if (existing != m_entries.end()) {
        m_currentUsage -= existing->cost;
        m_lruList.erase(existing->lruIter);
        m_entries.erase(existing);
    }

    // Evict LRU entries to make room.
    evictToFit(cost);

    // Insert at front (most recently used).
    m_lruList.push_front(key);
    Entry entry;
    entry.pixmap  = pixmap;
    entry.cost    = cost;
    entry.lruIter = m_lruList.begin();
    m_entries.insert(key, entry);
    m_currentUsage += cost;

    qCDebug(lcPageCache).nospace()
        << m_name << " insert page " << key.pageIndex
        << " (" << (cost / 1024) << " KB) — total "
        << m_entries.size() << " entries, "
        << (m_currentUsage / (1024 * 1024)) << " / "
        << (m_budget / (1024 * 1024)) << " MB";
}

// ────────────────────────────────────────────────────────────────────────────
// clearForDocument() — Remove all entries for a given document ID.
// ────────────────────────────────────────────────────────────────────────────

void PageCache::clearForDocument(quintptr docId) {
    int removed = 0;
    for (auto it = m_entries.begin(); it != m_entries.end(); ) {
        if (it.key().docId == docId) {
            m_currentUsage -= it->cost;
            m_lruList.erase(it->lruIter);
            it = m_entries.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    if (removed > 0) {
        qCDebug(lcPageCache) << m_name << "cleared" << removed
                             << "entries for doc" << docId
                             << "— remaining" << m_entries.size()
                             << "entries," << (m_currentUsage / (1024 * 1024)) << "MB";
    }
}

void PageCache::clearForDocumentExceptZoom(quintptr docId, int keepZoomPermille) {
    int removed = 0;
    for (auto it = m_entries.begin(); it != m_entries.end(); ) {
        const auto &key = it.key();
        if (key.docId == docId && key.zoomPermille != keepZoomPermille) {
            m_currentUsage -= it->cost;
            m_lruList.erase(it->lruIter);
            it = m_entries.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    if (removed > 0) {
        qCDebug(lcPageCache) << m_name << "cleared" << removed
                             << "stale-zoom entries for doc" << docId
                             << "(keeping zoom" << keepZoomPermille << ")"
                             << "— remaining" << m_entries.size()
                             << "entries," << (m_currentUsage / (1024 * 1024)) << "MB";
    }
}

// ────────────────────────────────────────────────────────────────────────────
// clear() — Wipe everything.
// ────────────────────────────────────────────────────────────────────────────

void PageCache::clear() {
    if (!m_entries.isEmpty()) {
        qCDebug(lcPageCache) << m_name << "clearing all"
                             << m_entries.size() << "entries ("
                             << (m_currentUsage / (1024 * 1024)) << "MB)";
    }
    m_entries.clear();
    m_lruList.clear();
    m_currentUsage = 0;
}

// ────────────────────────────────────────────────────────────────────────────
// logStats() — Diagnostic output.
// ────────────────────────────────────────────────────────────────────────────

void PageCache::logStats() const {
    qCDebug(lcPageCache).nospace()
        << m_name << " stats: " << m_entries.size() << " entries, "
        << (m_currentUsage / (1024 * 1024)) << " / "
        << (m_budget / (1024 * 1024)) << " MB ("
        << (m_budget > 0 ? (m_currentUsage * 100 / m_budget) : 0) << "%)";
}

// ────────────────────────────────────────────────────────────────────────────
// evictToFit() — Remove LRU entries until budget can accommodate the request.
// ────────────────────────────────────────────────────────────────────────────

void PageCache::evictToFit(qint64 requiredSpace) {
    while (m_currentUsage + requiredSpace > m_budget && !m_lruList.empty()) {
        const PageCacheKey &victimKey = m_lruList.back();
        auto victimIt = m_entries.find(victimKey);
        if (victimIt != m_entries.end()) {
            qCDebug(lcPageCache) << m_name << "evicting page"
                                 << victimKey.pageIndex
                                 << "(" << (victimIt->cost / 1024) << "KB)";
            m_currentUsage -= victimIt->cost;
            m_entries.erase(victimIt);
        }
        m_lruList.pop_back();
    }
}
