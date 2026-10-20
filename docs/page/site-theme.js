// The site's theme: the colours a scene that names no theme takes anywhere on the site, read from
// the page's own CSS at each call, so a scene loaded after a switch between light and dark takes
// the new ones. A colour left out is the runtime's light or dark one.
const COLORS = {background: "--stage", foreground: "--fg", muted: "--dim", accent: "--accent", surface: "--raise", line: "--line2"};

export function siteTheme() {
  const style = getComputedStyle(document.documentElement);
  const colors = Object.entries(COLORS).map(([color, name]) => [color, style.getPropertyValue(name).trim()]);
  return {dark: document.documentElement.dataset.theme !== "light", ...Object.fromEntries(colors)};
}

// Calls listener after each switch between the site's light and dark themes; returns a function
// that stops it.
export function onSiteTheme(listener) {
  const observer = new MutationObserver(() => listener());
  observer.observe(document.documentElement, {attributes: true, attributeFilter: ["data-theme"]});
  return () => observer.disconnect();
}
