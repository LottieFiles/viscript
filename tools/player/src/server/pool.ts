// WASM instances shared by every session of a render server.
//
// An engine in a loaded instance costs ~10 ms and its scene's memory, where an instance of its own
// costs ~16 MB more, so sessions share instances. Fonts go in once per instance, through an anchor
// engine kept for the instance's lifetime, because ThorVG shares fonts between the engines of an
// instance but drops them when its last engine goes. A name keeps the first font registered under
// it, so when the host's fonts change, the instances retire: their sessions keep them until they
// load again, and new sessions get a new one. An instance without sessions goes after `idleMs`,
// and one that trapped or aborted is never called into again.
import type {VisModule, VisScene} from "vis-wasm-bundle";
import type {FontType} from "../fonts/provider.ts";
import type {AssetRef} from "../protocol.ts";

/** A font to register: its bytes, and whether its outlines are TrueType or CFF. */
export interface FontAnswer {
    readonly data: ArrayBuffer;
    readonly type: FontType;
}

export interface PoolDeps {
    /** Starts a WASM instance; `onAbort` runs if it aborts later. */
    createModule(onAbort: () => void): Promise<VisModule>;
    /** A new engine in `module`, without a scene. */
    createEngine(module: VisModule): Promise<VisScene>;
    /** The font for a name, or null when there is none; asked once per name and instance. */
    font(name: string): Promise<FontAnswer | null>;
    assetBytes(asset: AssetRef): Promise<ArrayBuffer>;
}

export interface PoolOptions {
    /** Milliseconds an instance without sessions is kept for the next one. */
    readonly idleMs?: number;
}

/** A trap or abort leaves an instance in an unknown state. */
export const isTrap = (error: unknown): boolean => error instanceof WebAssembly.RuntimeError;

const isSound = (asset: AssetRef): boolean => asset.mime.startsWith("audio/");

class Instance {
    readonly ready: Promise<VisModule>;
    /** The host's fonts it was started with; see InstancePool.retire(). */
    readonly generation: number;
    /** Name → whether a font went in under it, or the registration under way. */
    readonly fonts = new Map<string, Promise<boolean>>();
    anchor: VisScene | null = null;
    leases = 0;
    idle: ReturnType<typeof setTimeout> | undefined;
    faulted = false;

    constructor(generation: number, start: (instance: Instance) => Promise<VisModule>) {
        this.generation = generation;
        this.ready = start(this);
    }
}

/** A session's engine in a shared instance. */
export interface Lease {
    readonly engine: VisScene;
    /** @internal */
    readonly instance: Instance;
    /** Asset name → id registered in this engine. */
    readonly assets: Map<string, string>;
    released: boolean;
}

export class InstancePool {
    readonly #deps: PoolDeps;
    readonly #idleMs: number;
    readonly #instances = new Set<Instance>();
    #generation = 0;

    constructor(deps: PoolDeps, {idleMs = 30000}: PoolOptions = {}) {
        this.#deps = deps;
        this.#idleMs = idleMs;
    }

    /** A new engine in the current instance, which is started if there is none. */
    async acquire(): Promise<Lease> {
        const instance = this.#current() ?? this.#create();
        clearTimeout(instance.idle);
        instance.idle = undefined;
        instance.leases++;
        try {
            const module = await instance.ready;
            this.#assertHealthy(instance);
            const engine = await this.#deps.createEngine(module);
            return {engine, instance, assets: new Map(), released: false};
        } catch (error) {
            if (isTrap(error)) this.#fault(instance);
            this.#leave(instance);
            throw error;
        }
    }

    /** Whether the lease's engine can still be called into. */
    usable(lease: Lease): boolean {
        return !lease.released && !lease.instance.faulted;
    }

    /** Whether the lease's instance takes another load; if not, the session moves to a new one. */
    fits(lease: Lease): boolean {
        return !lease.instance.faulted && lease.instance.generation === this.#generation;
    }

    /**
     * Registers a font under each of `names` that the lease's instance has not asked for yet, and
     * waits for those under way. Resolves true when any went in, so a scene loaded before them
     * has them when it loads again; text in the others stays in the default font. Rejects only
     * when the instance faulted.
     */
    async registerFonts(lease: Lease, names: readonly string[]): Promise<boolean> {
        const instance = lease.instance;
        const added = await Promise.all(names.map((name) =>
            (instance.fonts.get(name) ?? this.#register(instance, name)).catch(() => false)));
        this.#assertHealthy(instance);
        return added.some(Boolean);
    }

    /**
     * Registers the assets a scene loads in the lease's engine, replacing any of the same name.
     * Sounds go to the engine's audio when `audio` is set, and are left out otherwise.
     */
    async ensureAssets(lease: Lease, assets: readonly AssetRef[], audio = false): Promise<void> {
        const wanted = assets.filter((asset) => !isSound(asset) || (audio && lease.engine.audio));
        const missing = wanted.filter((asset) => lease.assets.get(asset.name) !== asset.id);
        const loaded = await Promise.all(missing.map(async (asset) => [asset, await this.#deps.assetBytes(asset)] as const));
        for (const [asset, data] of loaded) {
            if (lease.released || lease.assets.get(asset.name) === asset.id) continue;
            if (isSound(asset)) lease.engine.audio!.load(asset.name, data);
            else lease.engine.asset(asset.name, data, asset.mime);
            lease.assets.set(asset.name, asset.id);
        }
    }

    /** Passes on an error from a call into the lease's engine, so a trap faults its instance. */
    report(lease: Lease, error: unknown): void {
        if (isTrap(error)) this.#fault(lease.instance);
    }

    release(lease: Lease): void {
        if (lease.released) return;
        lease.released = true;
        try {
            // A faulted instance is not called into; its engine goes with it.
            if (!lease.instance.faulted) lease.engine.destroy();
        } catch (error) {
            // A trapped instance can trap again here.
            if (!isTrap(error)) throw error;
            this.#fault(lease.instance);
        } finally {
            this.#leave(lease.instance);
        }
    }

    /**
     * The host's fonts changed, so the fonts registered so far may not be the ones it gives now.
     * Instances without sessions go at once; the others go when their sessions leave, and take
     * no new ones.
     */
    retire(): void {
        this.#generation++;
        for (const instance of this.#instances) if (!instance.leases) this.#drop(instance);
    }

    /** Drops every instance, for a server shutting down. */
    dispose(): void {
        for (const instance of this.#instances) this.#drop(instance);
    }

    #current(): Instance | undefined {
        for (const instance of this.#instances) {
            if (!instance.faulted && instance.generation === this.#generation) return instance;
        }
        return undefined;
    }

    #create(): Instance {
        const instance = new Instance(this.#generation, async (self) => {
            try {
                const module = await this.#deps.createModule(() => this.#fault(self));
                self.anchor = await this.#deps.createEngine(module);
                return module;
            } catch (error) {
                // A failed instance leaves the pool, so the next session retries it.
                this.#fault(self);
                throw error;
            }
        });
        this.#instances.add(instance);
        return instance;
    }

    // The claim goes in at once, so concurrent loads wait on the same answer. A font that could
    // not be had is asked for again by a later load; one the engine refuses is taken as none.
    #register(instance: Instance, name: string): Promise<boolean> {
        const registration = Promise.all([instance.ready, this.#deps.font(name)]).then(([, font]) => {
            if (!font) return false;
            this.#assertHealthy(instance);
            if (!instance.anchor) return false;
            try {
                instance.anchor.font(name, font.data, font.type);
                return true;
            } catch (error) {
                if (isTrap(error)) {
                    this.#fault(instance);
                    throw error;
                }
                console.warn(`viscript: the font for "${name}" was not loaded:`, error);
                return false;
            }
        });
        instance.fonts.set(name, registration);
        registration.catch(() => { if (instance.fonts.get(name) === registration) instance.fonts.delete(name); });
        return registration;
    }

    #assertHealthy(instance: Instance): void {
        if (instance.faulted) throw new Error("The viscript runtime stopped after an error; load the scene again");
    }

    #fault(instance: Instance): void {
        instance.faulted = true;
        this.#instances.delete(instance);
    }

    #leave(instance: Instance): void {
        // A faulted instance already left the pool, and is not called into to destroy its anchor.
        if (--instance.leases > 0 || instance.faulted) return;
        if (instance.generation !== this.#generation) {
            this.#drop(instance);
            return;
        }
        instance.idle = setTimeout(() => {
            if (!instance.leases) this.#drop(instance);
        }, this.#idleMs);
    }

    #drop(instance: Instance): void {
        clearTimeout(instance.idle);
        if (!this.#instances.delete(instance) || instance.faulted) return;
        try { instance.anchor?.destroy(); } catch { /* the instance goes either way */ }
    }
}
