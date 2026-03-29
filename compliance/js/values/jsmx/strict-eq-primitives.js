// Repo-authored idiomatic `jsval` fixture for flattened primitive strict equality.

if (!(undefined === undefined)) {
  throw new Error('undefined should strictly equal itself');
}

if (!(null === null)) {
  throw new Error('null should strictly equal itself');
}

if (!(+0 === -0)) {
  throw new Error('+0 should strictly equal -0');
}

if ("same" !== "same") {
  throw new Error('equal strings should compare strictly equal');
}

if (NaN === NaN) {
  throw new Error('NaN should not strictly equal itself');
}

if (1 === "1") {
  throw new Error('number and string should not compare strictly equal');
}

if (true === 1) {
  throw new Error('boolean and number should not compare strictly equal');
}

if (null === undefined) {
  throw new Error('null and undefined should not compare strictly equal');
}
