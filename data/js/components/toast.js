// Lusol — lightweight toast notifications, no dependencies.

function stack() {
  let node = document.getElementById("toast-stack");
  if (!node) {
    node = document.createElement("div");
    node.id = "toast-stack";
    node.className = "toast-stack";
    document.body.appendChild(node);
  }
  return node;
}

export function toast(message, type = "info", timeout = 3600) {
  const node = document.createElement("div");
  node.className = `toast ${type}`;
  node.textContent = message;
  stack().appendChild(node);
  setTimeout(() => {
    node.style.transition = "opacity 200ms ease";
    node.style.opacity = "0";
    setTimeout(() => node.remove(), 200);
  }, timeout);
}
