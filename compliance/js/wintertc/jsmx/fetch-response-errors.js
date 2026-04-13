// Repo-authored WHATWG Fetch Response rejection fixture.
const captured = [];

try {
  Response.redirect("https://example.com", 200);
} catch (e) {
  captured.push(e.name);
}

try {
  Response.redirect("https://example.com", 404);
} catch (e) {
  captured.push(e.name);
}

try {
  new Response("body", { status: 99 });
} catch (e) {
  captured.push(e.name);
}

captured;
