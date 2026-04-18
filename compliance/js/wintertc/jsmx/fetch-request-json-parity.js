// Repo-authored Request.json() parity fixture.
//
// A JSON string body is consumed via json(), which parses
// it into a structured object. The nested array round-trips
// as a real array of numbers.

const req = new Request("https://ex.com", {
	method: "POST",
	body: '{"x":1,"y":[2,3]}',
});

// const obj = await req.json();
// obj.x === 1
// Array.isArray(obj.y) && obj.y.length === 2
// obj.y[0] === 2 && obj.y[1] === 3
// req.bodyUsed === true
