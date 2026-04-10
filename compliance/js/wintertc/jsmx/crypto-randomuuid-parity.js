// Repo-authored WinterTC randomUUID parity fixture.
const uuid = crypto.randomUUID();

[
  typeof uuid,
  uuid.length,
  uuid[8],
  uuid[13],
  uuid[14],
  uuid[18],
  uuid[19],
  uuid[23],
];
