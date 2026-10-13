#ifndef _VIS_AUDIT_H_
#define _VIS_AUDIT_H_

// Audit allowances identify exact scene-path/Text endpoints with a matched/used ledger.
// Duplicate, unmatched or unused allowances fail; repeated IDs retain containment checks
// and Rectangle identity must be unique within a scene. Sampling includes the final
// endpoint; maxSamples is checked before inventory construction.
int visAudit(int argc, char** argv) noexcept;

#endif
