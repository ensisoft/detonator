# DETONATOR 2D

## Coding Conventions and Design 💭
 
- todo: c++ objects that represent classes
- todo: asserts/bugs/exceptions
- todo: callbacks
  - recursion guard
  - callbacks from Lua to engine  
- todo: UX conventions
- todo: logging conventions
- todo: content loading/content errors


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
 