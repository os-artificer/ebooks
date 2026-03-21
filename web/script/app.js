const { createApp, ref, computed, onMounted } = Vue;

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
  // srcset format: "url [descriptor], url [descriptor], ..."
  // We keep descriptors after each URL as-is.
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

/** 欢迎页（右侧文章区默认展示） */
const WELCOME_HREF = "./page/welcome.html";

function rewriteRelativeUrls(container, baseUrl) {
  // Common attributes
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

async function fetchAndInject(href) {
  const container = document.getElementById("article");
  if (!container) return;

  container.innerHTML = '<div style="color:#6b7280;padding:24px 0;">加载中...</div>';

  const baseUrl = new URL(href, window.location.href).href;
  const res = await fetch(href, { cache: "no-store" });
  if (!res.ok) throw new Error(`HTTP ${res.status}`);

  const html = await res.text();
  const doc = new DOMParser().parseFromString(html, "text/html");
  if (!doc.body) throw new Error("Invalid HTML: missing body");

  container.innerHTML = doc.body.innerHTML;
  rewriteRelativeUrls(container, baseUrl);
}

createApp({
  template: getAppTemplate(),
  setup() {
    const activeHref = ref(null);
    const categories = ref([]);
    const searchQuery = ref("");

    const searchTrimmed = computed(() => searchQuery.value.trim());

    const filteredCategories = computed(() => {
      const q = searchTrimmed.value;
      if (!q) return categories.value;
      const lower = q.toLowerCase();
      return categories.value
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

    async function loadNavConfig() {
      // 与 index 同站部署：nav 放在 web/articles/，避免 ../articles 在仅发布 web 时 404
      const navUrl = new URL("./articles/nav.json", window.location.href).href;
      const response = await fetch(navUrl, { cache: "no-store" });
      if (!response.ok) throw new Error(`导航配置读取失败：HTTP ${response.status}`);
      const data = await response.json();
      if (!data || !Array.isArray(data.categories)) {
        throw new Error("导航配置格式错误：缺少 categories 数组");
      }
      categories.value = data.categories;
    }

    async function openArticle(href) {
      activeHref.value = href;
      try {
        await fetchAndInject(href);
      } catch (e) {
        const container = document.getElementById("article");
        if (container) {
          container.innerHTML = `<div style="color:#b91c1c;padding:24px 0;">加载失败：${String(e && e.message ? e.message : e)}</div>`;
        }
      }
    }

    function onToggle(cat, e) {
      if (searchTrimmed.value) return; // 搜索时强制展开，不写入 cat.open
      cat.open = Boolean(e.target && e.target.open);
    }

    onMounted(() => {
      // Default view
      loadNavConfig().catch((e) => {
        const container = document.getElementById("article");
        if (container) {
          container.innerHTML = `<div style="color:#b91c1c;padding:24px 0;">${String(e && e.message ? e.message : e)}</div>`;
        }
      });
      openArticle(WELCOME_HREF);
    });

    return {
      categories,
      filteredCategories,
      searchQuery,
      searchTrimmed,
      activeHref,
      welcomeHref: WELCOME_HREF,
      openArticle,
      onToggle,
    };
  },
}).mount("#app");

