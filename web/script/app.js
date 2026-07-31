const { createApp, ref, computed, onMounted, nextTick } = Vue;

// ---- Mermaid 图表渲染（文章内 ```mermaid 代码块）----
if (window.mermaid) {
  window.mermaid.initialize({
    startOnLoad: false,
    theme: "default",
    securityLevel: "loose", // 允许 <br/> 等 HTML 标签
  });
}

// 将注入后的文章中 `code.language-mermaid` 转换为 div.mermaid 并渲染新增节点
function renderMermaid(container) {
  if (!window.mermaid) return;
  container
    .querySelectorAll("pre > code.language-mermaid")
    .forEach((code) => {
      const pre = code.parentElement;
      const div = document.createElement("div");
      div.className = "mermaid";
      div.textContent = code.textContent;
      pre.replaceWith(div);
    });
  const nodes = Array.from(container.querySelectorAll(".mermaid"));
  if (nodes.length) {
    try {
      window.mermaid.run({ nodes });
    } catch (e) {
      console.error("mermaid render error", e);
    }
  }
}

function getAppTemplate() {
  const el = document.getElementById("vue-app-template");
  if (!el) {
    throw new Error("缺少 #vue-app-template：请在 index.html 中提供 Vue 模板");
  }
  const html = el.innerHTML.trim();
  el.remove();
  return html;
}

function absUrl(value, baseUrl) {
  if (!value) return value;
  try {
    return new URL(value, baseUrl).href;
  } catch {
    return value;
  }
}

function rewriteSrcset(srcsetValue, baseUrl) {
  if (!srcsetValue) return srcsetValue;
  return srcsetValue
    .split(",")
    .map((part) => part.trim())
    .filter(Boolean)
    .map((part) => {
      const pieces = part.split(/\s+/);
      const url = pieces[0];
      if (!url) return part;
      const abs = absUrl(url, baseUrl);
      return [abs, ...pieces.slice(1)].join(" ");
    })
    .join(", ");
}

/** 欢迎页（无文章时回退；二维码入口） */
const WELCOME_HREF = "./page/welcome.html";

/** 首页「全部」下每个分类默认展示的文章数 */
const HOME_SECTION_LIMIT = 5;

function rewriteRelativeUrls(container, baseUrl) {
  container.querySelectorAll("img[src]").forEach((el) => {
    el.setAttribute("src", absUrl(el.getAttribute("src"), baseUrl));
  });

  container.querySelectorAll("img[srcset]").forEach((el) => {
    const srcset = el.getAttribute("srcset");
    el.setAttribute("srcset", rewriteSrcset(srcset, baseUrl));
  });

  container.querySelectorAll("source[srcset]").forEach((el) => {
    const srcset = el.getAttribute("srcset");
    el.setAttribute("srcset", rewriteSrcset(srcset, baseUrl));
  });

  container.querySelectorAll("source[src]").forEach((el) => {
    el.setAttribute("src", absUrl(el.getAttribute("src"), baseUrl));
  });

  container.querySelectorAll("a[href]").forEach((el) => {
    el.setAttribute("href", absUrl(el.getAttribute("href"), baseUrl));
  });

  container.querySelectorAll("link[href]").forEach((el) => {
    el.setAttribute("href", absUrl(el.getAttribute("href"), baseUrl));
  });
}

/**
 * href like "./page/cpp/foo.html" → hash path "page/cpp/foo.html"
 */
function hrefToHash(href) {
  if (!href) return "";
  return String(href)
    .replace(/^\.\//, "")
    .replace(/^\/+/, "");
}

/**
 * hash "#/page/cpp/foo.html" or "#page/..." → "./page/cpp/foo.html" or null for home
 */
function hashToHref(hash) {
  if (!hash) return null;
  let path = String(hash).replace(/^#/, "").replace(/^\/+/, "");
  if (!path || path === "/") return null;
  if (!path.startsWith("page/")) {
    // allow full relative forms already including ./
    if (path.startsWith("./page/")) return path;
    return null;
  }
  return "./" + path;
}

function setHashForHref(href) {
  const path = hrefToHash(href);
  const next = path ? "#/" + path : "#/";
  if (window.location.hash !== next) {
    window.location.hash = next;
  }
}

async function fetchAndInject(href) {
  const container = document.getElementById("article");
  if (!container) return;

  container.innerHTML =
    '<div style="color:#6b7280;padding:24px 0;">加载中...</div>';

  const baseUrl = new URL(href, window.location.href).href;
  const res = await fetch(href, { cache: "no-store" });
  if (!res.ok) throw new Error(`HTTP ${res.status}`);

  const html = await res.text();
  const doc = new DOMParser().parseFromString(html, "text/html");
  if (!doc.body) throw new Error("Invalid HTML: missing body");

  container.innerHTML = doc.body.innerHTML;
  rewriteRelativeUrls(container, baseUrl);
  renderMermaid(container);
}

createApp({
  template: getAppTemplate(),
  setup() {
    const view = ref("home"); // 'home' | 'article'
    const activeHref = ref(null);
    const activeCategory = ref("all"); // 'all' | category.key
    const categories = ref([]);
    const searchQuery = ref("");
    let suppressHashWrite = false;

    function withHashSuppressed(fn) {
      suppressHashWrite = true;
      try {
        fn();
      } finally {
        // hashchange is sync in most browsers when setting location.hash
        queueMicrotask(() => {
          suppressHashWrite = false;
        });
      }
    }

    const searchTrimmed = computed(() => searchQuery.value.trim());

    const filteredCategories = computed(() => {
      let list = categories.value;
      if (activeCategory.value !== "all") {
        list = list.filter((cat) => cat.key === activeCategory.value);
      }
      const q = searchTrimmed.value;
      if (!q) return list;
      const lower = q.toLowerCase();
      return list
        .map((cat) => ({
          ...cat,
          items: (cat.items || []).filter((it) =>
            String(it.text || "")
              .toLowerCase()
              .includes(lower)
          ),
        }))
        .filter((cat) => cat.items.length > 0);
    });

    /** 首页分节：仅在「全部」且无搜索时截断为前 N 篇，并标记是否进入专题 */
    const homeSections = computed(() => {
      const capAll =
        activeCategory.value === "all" && !searchTrimmed.value;
      return filteredCategories.value.map((cat) => {
        const allItems = cat.items || [];
        const total = allItems.length;
        const hasMore = capAll && total > HOME_SECTION_LIMIT;
        return {
          key: cat.key,
          title: cat.title,
          total,
          hasMore,
          items: hasMore ? allItems.slice(0, HOME_SECTION_LIMIT) : allItems,
        };
      });
    });

    const activeCategoryMeta = computed(() => {
      if (!activeHref.value) return null;
      for (const cat of categories.value) {
        if ((cat.items || []).some((it) => it.href === activeHref.value)) {
          return cat;
        }
      }
      return null;
    });

    const siblingItems = computed(() => {
      const cat = activeCategoryMeta.value;
      if (!cat) return [];
      const q = searchTrimmed.value;
      const items = cat.items || [];
      if (!q) return items;
      const lower = q.toLowerCase();
      return items.filter((it) =>
        String(it.text || "")
          .toLowerCase()
          .includes(lower)
      );
    });

    async function loadNavConfig() {
      const navUrl = new URL("./articles/nav.json", window.location.href).href;
      const response = await fetch(navUrl, { cache: "no-store" });
      if (!response.ok) {
        throw new Error(`导航配置读取失败：HTTP ${response.status}`);
      }
      const data = await response.json();
      if (!data || !Array.isArray(data.categories)) {
        throw new Error("导航配置格式错误：缺少 categories 数组");
      }
      categories.value = data.categories;
    }

    function findCategoryKeyForHref(href) {
      for (const cat of categories.value) {
        if ((cat.items || []).some((it) => it.href === href)) {
          return cat.key;
        }
      }
      return null;
    }

    async function openArticle(href, { syncHash = true } = {}) {
      view.value = "article";
      activeHref.value = href;
      const catKey = findCategoryKeyForHref(href);
      if (catKey) {
        activeCategory.value = catKey;
      }
      if (syncHash) {
        withHashSuppressed(() => setHashForHref(href));
      }
      try {
        await fetchAndInject(href);
        // Scroll article pane to top
        const articleEl = document.getElementById("article");
        if (articleEl) articleEl.scrollTop = 0;
        const main = document.querySelector(".site-main");
        if (main) main.scrollTop = 0;
      } catch (e) {
        const container = document.getElementById("article");
        if (container) {
          container.innerHTML = `<div style="color:#b91c1c;padding:24px 0;">加载失败：${String(
            e && e.message ? e.message : e
          )}</div>`;
        }
      }
    }

    function goHome({ syncHash = true } = {}) {
      view.value = "home";
      activeHref.value = null;
      if (syncHash) {
        withHashSuppressed(() => {
          window.location.hash = "#/";
        });
      }
      const main = document.querySelector(".site-main");
      if (main) main.scrollTop = 0;
    }

    function selectCategory(key) {
      activeCategory.value = key;
      if (view.value === "article") {
        goHome();
      }
    }

    async function applyHashRoute() {
      if (suppressHashWrite) return;
      const href = hashToHref(window.location.hash);
      if (!href) {
        if (view.value !== "home") goHome({ syncHash: false });
        return;
      }
      // welcome or article
      if (activeHref.value === href && view.value === "article") return;
      await openArticle(href, { syncHash: false });
    }

    onMounted(async () => {
      try {
        await loadNavConfig();
        window.addEventListener("hashchange", () => {
          applyHashRoute();
        });
        const initial = hashToHref(window.location.hash);
        if (initial) {
          await openArticle(initial, { syncHash: false });
        } else if (
          !categories.value.length ||
          !categories.value.some((c) => c.items && c.items.length)
        ) {
          await openArticle(WELCOME_HREF, { syncHash: true });
        } else {
          goHome({ syncHash: false });
          if (!window.location.hash) {
            // keep clean URL on first visit
          }
        }
      } catch (e) {
        view.value = "article";
        await nextTick();
        const container = document.getElementById("article");
        if (container) {
          container.innerHTML = `<div style="color:#b91c1c;padding:24px 0;">${String(
            e && e.message ? e.message : e
          )}</div>`;
        }
      }
    });

    return {
      view,
      categories,
      filteredCategories,
      homeSections,
      searchQuery,
      searchTrimmed,
      activeHref,
      activeCategory,
      activeCategoryMeta,
      siblingItems,
      welcomeHref: WELCOME_HREF,
      hrefToHash,
      openArticle,
      goHome,
      selectCategory,
    };
  },
}).mount("#app");
