export function isReleaseVersion(value) {
  return typeof value === 'string' && value.trim() === value && /^(0|[1-9]\d*)\.(0|[1-9]\d*)$/.test(value)
}

export function getDocsRoute(version) {
  if (version === 'trunk') return 'docs/trunk/'
  if (!isReleaseVersion(version)) throw new Error(`Invalid documentation version: ${version}`)
  return `docs/v${version}/`
}

export function sortDocsVersions(versions) {
  if (!Array.isArray(versions)) throw new Error('Documentation versions must be an array.')
  const seen = new Set()
  for (const version of versions) {
    if (version !== 'trunk' && !isReleaseVersion(version)) {
      throw new Error(`Invalid documentation version: ${version}`)
    }
    if (seen.has(version)) throw new Error(`Duplicate documentation version: ${version}`)
    seen.add(version)
  }
  if (!seen.has('trunk')) throw new Error('Documentation versions must include trunk.')

  return [...versions].sort((left, right) => {
    if (left === right) return 0
    if (left === 'trunk') return 1
    if (right === 'trunk') return -1
    const leftParts = left.split('.').map(BigInt)
    const rightParts = right.split('.').map(BigInt)
    for (let index = 0; index < leftParts.length; index++) {
      if (leftParts[index] !== rightParts[index]) return leftParts[index] > rightParts[index] ? -1 : 1
    }
    return 0
  })
}
