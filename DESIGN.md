DETONATOR 2D 💥💣
===================

## Software Design Document 💭

## C++ Objects That Represent User Types
In a game engine there's a typically a need to let the user define new game content and create new resource types such as
scenes, entities, UIs and materials. For example the user might want to define a material called "marble" and there needs
to be way to represent the properties of  "marble" once and to be able to create multiple new instances of "marble" during
the game play. 

The way this is accomplished in DETONATOR 2D is by creating a C++ classes that are capable of representing new types in
data driven manner. So for example for materials we have a class called MaterialClass. The class contains all the properties
needed to represent a new material type. And for each new type of a material there exists exactly one MaterialClass C++
object. When the user edits the material in the editor they'll be manipulating the properties of this one single C++ object.
The class object allows the state to be loaded and saved to/from disk and both the editor and the game runtime loader
perform such loading and saving operations.

However this is not yet enough since during the game play we also need to create instances of of the user defined types.
For example if the material type is "marble" there can be various game objects that use the "marble" material but each object
has its own instance of marble. Essentially this is the same as having some real life paint with certain properties
and then having an infinite number of paint buckets containing the said paint. Each paint bucket refers/contains the same paint
with same properties but each bucket is a separate instance.

[graphics/material.h](graphics/material.h)

```
class MaterialClass 
{
public:
   // the public material class API for manipulating the type. 
private:
   std::string mID;
   std::string mName;
   // ... uniforms, samplers
}

class MaterialInstance
{
public:
  MaterialInstance(std::shared_ptr<const MaterialClass> klass)
  {...}

   // the public *instance* API
private:
  std::shared_ptr<const MaterialClass> mClass;
  float mMaterialTime;
  // other *instance* data
}
```

```
// design time material creation in the material editor
auto marble = std::make_shared<MaterialClass>();
marble->SetId("1234");
marble->SetName("My Marble");
marble->SetBaseColor(gfx::Color::DarkRed);
...

// game time instances in the game renderer
auto material = mLoader->FindMaterialClass(material_id);

auto m0 = gfx::CreateMaterialInstance(material);
m0->SetTime(game_time);

auto m1 = gfx::CreateMaterialInstance(material);
m1->SetTime(game_time);
   
```


To re-iterate, at runtime a user defined material type is represented by a *single* instance of MaterialClass and instances
of that type are represented by instances of MaterialInstance which each point to the type instance. (Confusing, right?)

This design allows the following:
- Static type information is separate from the instance information.
  This allows many lightweight instances to refer to the same static data without repeating the data per each instance.
- It's clear which information is static and which is runtime only. I.e. when loading/saving classes from/to JSON
  each MaterialClass member is loaded/saved.
- Game runtime operations only apply to MaterialInstance and all MaterialClass objects are immutable.
- Design time operations apply only to MaterialClass objects.


## Error Handling
Error handling is a hot topic and many people and even programming environments try to solve the issues in some
specific way. The problem comes when a "one solution fits all" type of approach is forced to deal with several
different categories of possible software issues/errors. 

Personally I prefer not to lump all potential software issues into a single category but to have a better classification
and distinction between the typical issues. Once this is established it then makes to sense to assume that the there would
be also different ways and strategies to deal with the specific issues.

### Bugs
Bugs are errors in the program itself, i.e. mistakes made by the programmer. A bug condition means that the program
state and its subsequent logic are no longer well defined and inside the permitted state. All possible computations
are thus invalid and results are expected to be garbage. A typical example is OOB array access, nullptr access and state 
violations such as method pre- and post conditions and invariants that must be held.

In my opinion the best strategy is to simply flat out refuse to deal with bugs. I.e. any possible "workaround" is only
going to muddle the program logic and mask the original problem thus make it harder to debug and understand what was
the root cause of the problem. Rather a better strategy is to catch any and all of such issues as close as possible
and produce a loud and clear program termination with stack trace and debuggable core dump for post-mortem analysis.

This in turn means that bugs are shaken out rather quickly and bug conditions are very obvious and as easy to debug
as possible by preserving the offending call stack and program state. 

*Once you accept and get over the "scary" idea of dumping core this strategy really yields to robust
as-simple-as-possible code that is straightforward to debug. Bug conditions are loud and clear with BUGS and ASSERTS
and there's no need to try to write any logic around what is fundamentally broken.*

[base/assert.h](base/assert.h) offers two macros for helping the programmer to validate the program and to produce 
controlled termination on bugs.

#### ASSERT(expr)
- Similar to the standard assert macro.
- Always built-in, so never omitted. 
- Tries to detect the presence of a debugger 
  - If theres' a debugger trap is raised so that debugger breaks and live debugging can be performed.
  - If there's no debugger a core dump is produced, the current thread's callstack is written stdout and abort is called.
- Use this the same as the standard assert, i.e. to validate program state, etc. 

```
  ASSERT(!chunk->HasValue("type"));
```

#### BUG(msg)
- Essentially the same as ASSERT except that there's no expression/condition to be evaluated. Instead every invocation
  produces a controlled program abort (with core dump and stack trace) unconditionally.
- Use this in cases of fallthrough, or any such code path that "should not happen".

```
  if (foo == 1)
     ...
  else if (foo == 2)
    ...
  else BUG("Missing foo case");     
```

### Errors
Errors are conditions that are not BUGS, but rather logical conditions related to the content and the runtime operation
of the program, that are expected to fail occasionally. So in other words ERRORS are not errors in the program itself
but a part of the logical and possible valid state of the program. A program that enters an ERROR state hasn't bugged out
but is in a well-formed state. The error is thus only really an error from the user perspective.  For example if I open
a web browser and try to access a resource that doesn't exist the browser will produce a 404 error. The browser itself
functions correctly and the error is only a content error and is only really an error from my point of view.

Some more examples are:
- File open failed, file not found, no permission etc. similar issues.
- Network socket connection failures, DNS resolution failures etc.
- Content and data errors, i.e. files containing garbage unexpected mal-formed content etc.

Typically the program needs to handle these in a way that doesn't abort the program but possibly tries to do something
such as re-trying to operation , logging the issue and/or notifying the user that something went wrong. 
This implies that a good strategy to deal with these is to use something such as status codes, or status objects
that allow simple and straightforward logic to be written to check for an failure condition and reacting appropriately.

### Exceptions
Finally there are scenarios when something unexpected happens. Typically these are unexpected resource allocation failures.
For example it might happen that the OS fails to allocate a TCP socket object, a mutex, a file handle etc. These conditions
might be rare but they can still happen. 

Arguably the best way to deal with these is to throw C++ exceptions.

- Using error codes would muddle the interfaces and conflate error conditions with exceptional conditions.
- Using a ctor to allocate an OS level resource is idiomatic and throwing an exception is the only way to indicate failure.
- Typically very places are able to catch so the propagation might have to jump over large swathes of code up the stack.
  Using error codes would be very tedious and would leak internal details from lower layers in the public interfaces.

Typical way to structure the code
- Have a high level try { } catch inside some logical boundary such as the current thread "main".
- Make intermediate code exception aware and exception safe (idiomatic C++ wrt exception safety).
- Take action after having caught the exception.
- Hard to come up with good "retry" strategies other than "free" large swathes of program state and re-try.
- Controlled program exit might make sense.

## Logging Guidelines

The information below only pertains to the part of the code that is part of the runtime game i.e. basically everything 
that isn't the editor. However, the editor itself has very similar system in and follows the same ideas closely.

The important thing to keep in mind is that the logs are designed towards the engine and the game programmer
instead of the end users. So anything that requires the end user attention whether in the editor or in the game itself
should use some proper UI mechanism.

In general be careful when adding any logging calls and avoid putting repeated log statements inside the render loop or 
inside the update loop without any kind of "do once" condition. Spamming logs inside the hot loop will create a lot of 
noise, expand the log volume considerably and generally make things unusable. Currently there's no  built-in "log once"
functionality in the logging system itself but every case has to solved separately. An example how this is done 
regrading UI style properties and materials can be found in engine/ui.h and engine/ui.cpp where the styling system has
a map of properties and materials that have failed and uses those maps to limit logging to only one log print.

The utilities in [base/logging.h](base/logging.h) offer various levels of logging functionality. DEBUG, INFO, WARN and ERROR. 

### DEBUG
This macro expands to print debug information that is mostly designed to help the engine programmer understand
what is happening inside the engine itself. By default the editor turns the debug logging on and prints everything
to the stdout in the associated console/terminal. The distributable engine/game package will have the debug logging 
turned off by default but the --debug-log cmd line parameter can be used to turn it on.

```
  $ ./MyGame --debug-log true 
```

Typical use cases are to document one off events such as
- Some resource got created, loaded, updated, deleted
- Some interesting state changed such as 
  - game play started, stopped
  - engine subsystem started, stopped,
  - audio playback started, stopped
  - animation state started, stopped
  - etc.

```
  DEBUG("Program was built successfully. [name='%1', info='%2']", mName, build_info);
```

### INFO
This macro is informational message that is maybe necessary to be printed but not particularly useful with respect
to the execution of the program itself. 

Typical uses are pretty much just:
- Copyright and license information
- Engine/game build dates, versions
- Other nice to have information
  - OpenGL version, device vendor, vendor version etc.

### WARN
*See the discussion about error handling regarding errors*

This macro produces a warning message and the distinction between a warning and an error is that with a warning it's 
still possible to continue the operation but the outcome might not be perfect. In other words a warning is a warning
related to an issue in the application content or runtime behaviour when something was not 100% possible but the
operation can proceed by applying defaults or simply by omitting some particular aspect of the behaviour.

For example the UI styling system provides defaults for all materials and style properties. If some user defined style
doesn't provide a value for some style property the rendering can still proceed by using the default built-in value. 
In this case the engine will produce warning for the missing style property and visual output will likely suffer.

```
  WARN("Program uses more textures than there are units available.");
```

### ERROR
*See the discussion about error handling regarding errors*

This macro produces an error message and the distinction between a warning and an error is that with an error it's 
not possible to continue the current operation in any meaningful way. 

For example if the user specified shader fails to compile and the program cannot be built the rendering operation
cannot proceed. We would then want to log the ERROR regarding the shader compilation and subsequently return early
from the rendering call since nothing else can be done.

```
  ERROR("Program build error. [name='%1', error='%2']", mName, build_info);
```

## Lua Integration and Game Error Handling

This is specifically about our perspective on the game code, i.e. the Lua code that is written by the game developer.  The failure models are:

**1. The Lua code is not valid Lua code and contains for example syntax errors**

**2. The Lua code calls an engine API that doesn't exist**

**3. The Lua code calls an engine API with wrong parameters**
 - Incorrect types for arguments
 - Incorrect argument values such as int flag values, or string flag names
 - OOB values for array/vector/container etc. indices
 - Nil objects when a valid object is needed

**4. The Lua code tries to access some data (such as a scripting variable) that is defined as part of the resource type**

**5. The Lua code calls the engine API *correctly* but the call fails with a  content error such as as missing resource**

### Resolution

Cases (1) and (2) should be  handled by the Lua interpreter and sol3 and there should be nothing that the engine side Lua integration code needs to do. 
The Lua code cannot continue but should abort and produce a nice error message/stack trace. Ideally this would not happen during game runtime
at all but at "compile" time as much as possible. (Compile time being some validation step in the editor after/during the code has been written)

Case (3) is handled partially by Lua/sol3. When the expected parameter types don't match the outcome should be the same than with (1) and (2).
However the remaining case is something like a flag value (either by int or by string) has an incorrect value. In a statically checked language
this case would not normally exist but would also be a compile time error.  It seems that the best strategy here is to treat these as "compile" 
errors and should abort/panic the Lua interpreter the same way 1. and 2. do. However, this check must be done manually in the engine and a 
hard error (panic) needs to be produced. 

Case (4) is open for debate. There are 2 sides to this story. 
  - It's nice to have rigidity and checking here to eliminate simple issues such as typos causing hard to spot bugs. 
  - It's sometimes a bit annoying to have to go back to the entity/scene resource to add a new variable that is needed.

Right now I'm thinking the ideal solution here would be to allow "best of both worlds". Make the access to the variables be checked for existence, 
types and other access flags (read-only, private) but make it simple to automate the addition of a new variable. I.e. when the Lua code is written
and if the variable doesn't yet exist in the resource type (entity etc) the user could be prompted to create it automatically thus providing an
improved UX and smoother code writing experience. Unfortunately this is very hard to implement properly since Lua lacks the required typing.
Yet I'm still going to prefer more rigid approach and cumbersome approach to eliminate needless bugs than spending time later on chasing down
typos and other such non-sense.

Case (5) is actually only an error for the game developer. From the engine perspective this isn't an error. The rationale is as follows:
  - Essentially this is the same as a statically typed/compiled application passing the compile/build step but failing at runtime
    because some resource such as a file could not be opened. In most applications this is within the expected outcomes
    that the application programmer needs to handle and write logic about. In other words in such an application 
    not being able to open some file is not an error in the application itself (i.e. it's not a BUG) but part of the
    (expected) valid states and operation of the program.
  - This  type of 'error' doesn't invalidate the program syntax wise. The program code itself is still well formed
    (assuming no other such issues exist). 

It seems that the best strategy here (from the engine perspective) is to let the game code continue to execute. 
In the engine code we should:
- Log the error with warning/error logs as appropriate
- Return a success/error code/value to the caller. 

It's then up to the game programmer to decide whether they should assert/abort or continue with the execution of their game program. 
The important takeaway is that turning this error into a hard error (panic) in the engine code makes it inconvenient
for the game developer write code and work on the game. It's very possible that some resource is not available and cannot be found as the game
is being worked on and resources are edited/deleted/added but doing a hard fail here always blocks the game from
running further and that is a decision we can't make.

Random thoughts:

- Bugs in the engine should be handled differently from bugs in the game code. I.e. bugs in engine should hit BUGS and ASSERTS. 
- It's nice to have safety but we should also consider the cost of it. Adding checks for the running game is nice when development 
  but maybe should it be possible to have an "unchecked" code path as well?
- Currently OOB checking on indices is partial because of efficiency. The penalty needs to be measured and then decided what would be right thing to do.


## Miscellaneous

### UX Conventions in the Editor
todo:
### Callbacks Suck
todo:
### Recursion Guard
todo:
### Content Errors
todo:

### Content Versioning

### Picking up Runtime Edits