export type PreviewUri = {
    scheme: string;
    path: string;
    authority?: string;
    query?: string;
    fragment?: string;
};

export function previewUriCandidate<T extends PreviewUri>(explicit: unknown, active: T | undefined): PreviewUri | T | undefined {
    if (!explicit || typeof explicit !== "object") return active;
    const value = explicit as Partial<PreviewUri>;
    return typeof value.scheme === "string" && typeof value.path === "string" ? value as PreviewUri : active;
}
