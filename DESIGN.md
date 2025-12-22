# Requirements

* A game engine for building 2D isometric ARPG games  
  * Maybe we could support RTS games technically? I think the tooling would be useful within our game and those typically follow a very similar perspective and map design to what we are building  
  * Thinking about RTS has me thinking … maybe we change the movement system to click to move? The pathfinding is broadly useful for things like collision detection with the terrain and then directly translates to enemies  
* A focus on debuggability, stability, and performance  
  * Builds, boots, and loads very fast  
  * [Tiger style dev](https://github.com/tigerbeetle/tigerbeetle/blob/main/docs/TIGER_STYLE.md)?  
    * Assert on negative space, crashing the application  
    * Fuzz the crap out of the mother trucker  
  * Able to attach a debugger?  
* “High-Code” experience  
  * Avoid tooling that makes things more complicated and less performant for the purpose of removing the developers need to … understand the basics of programming  
  * APIs over UIs  
* Hot Reloadable?  
  * I think hot reload might be a trap  
  * What might be more in line with the vision here is save-states since the game will be fast to boot and load back in  
* Asset Pipeline  
  * Supports importing maps from Tiled  
  * Supports sprite sheets (Aseprite integration?)  
* Physics Engine?  
  * I don't know if we need one at all, but we can use a library for this if needed   
* Networking?  
  * The game we are making needs to support four players fighting together in a PvE game  
  * The game needs matchmaking  
* Platform support?  
  * PC (Windows/Linux)  
  * Any other platforms are nice to have but not required  
* Level Editor?  
  * Might require custom meta-data in Tiled to be able to place objects that have behavior in our game (interactables, spawners, etc.)  
  * Will need to consider the experience of editing a map in Tiled and wanting to play-test it as you go  
    * This might be a non-issue if the game builds, reboots, and reloads very fast; just save and restart  
  * How do we handle things that *change* the environment without a level editor? Like where our ship crash lands?  
    * Might be able to handle this with a *Scene Graph* \- essentially we can associate a point, or an event with a traversal from one map to another  
* Nice to have features (that I’m thinking about right now)  
  * Foreground/Background elements with parallaxing (I’m thinking about this, because I’m not quite visualizing how to put these together using Tiled layers)


  # Decisions

* Programming Language  
  * C++  
* Networking  
  * Abstract over what’s used so we can change it later  
  * Start with P2P, but keep the engine unaware of where the host is  
    * Need to handle when the host disconnects and needs to be reassigned  
  * ENet seems promising: [http://enet.bespin.org/](http://enet.bespin.org/)  
  * Steamworks API for extending the functionality of ENet with lobbying etc  
* Graphics API  
  *  SDL2 \+ OpenGL  
* Level Editor  
  * Use Tiled for building out maps, including all elements like laying in invisible game objects  
  * Utilize metadata in Tiled to determine behavior (visibility, etc.)  
* Sprites (including Players, Enemies, Hitboxes, Hurtboxes, Particle Effects, etc.)  
  * Sprites will all be made with Aseprite, and we’ll build an Asset Pipeline (along with some conventions) that make importing from Aseprite seamless.  
  * Designers will need to define not only the Sprites in Aseprite, but also the invisible elements like hitboxes and hurtboxes  
    * We can use two techniques to align elements:  
      1. You can make the assets have identical canvas sizes, and align them by center-point  
      2. You can make an anchor point that matches an anchor point in another asset based on hex color codes  
* UI Builder  
  * [https://rive.app/](https://rive.app/)   
* Games built on the engine don’t need to think about the render and update loops and instead operate strictly over an asynchronous event-based architecture  
  * Why?  
    * Needing to obey the update/render loops of the engine without pub-sub would force a lot down on the *technical design* of games built on the engine. Once you start considering pub-subbing the loops, then it just makes sense to have a design that abstracts over them and instead exposes a more well-thought-out subscriber API.  
    * The main thing that jumps out to me is MonoBehavior classes from Unity, and Unity basically takes your object lifecycle hostage. This totally rules out objects that only last for the duration of a request. They generally need to do this so that the engine has access to a bean for that class that it can call the update/start/etc methods on.   
    * Rather than having bean soup (sub to everything or else\!) we can just … allow game devs to just subscribe to exactly what they need when they need it (expressed in terms they care about), and that sounded like a better design  
  * **Games built on the engine run at a fixed 60 FPS**—this will ensure that our devs   
    * The update loop will ALWAYS fire events  
    * The **render loop will drop frames that aren’t ready in 16ms**


  

# Work Breakdown
* Basic networking  
* Update/Render loops with SSE design for client API  
* Asset Pipeline  
  * Tiled asset pipeline  
    * Custom Tags for assigning behavior to Tiled objects/layers  
  * Rive asset pipeline  
  * Aseprite pipeline  
* Consider using an AI driven pipeline as well
