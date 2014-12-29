### Ren Garden

Ren Garden is a novel console for working with Rebol and Red code evaluation.  It uses C++ widgets from the Qt5 toolkit, and calls into the evaluator via the C++ binding Rencpp.  Under the hood, the console is implemented as a Qt rich text editor; and it has many features including:

	* Evaluations run on a thread separate from the GUI, allowing for responsiveness and cancellation of evaluations
	* Switching between single and multi-line editing, with a respectable suite of multi-line editing features
	* Stable copy-and-pasting from previous input while a command is running
	* Ability to undo an evaluation's output and return the console to the state of input prior to the evaluation
	* Dynamic addition of watch variables and expressions through a WATCH dialect

This work-in-progress is being pre-released under the GPLv3 for community review.  It is temporarily being managed inside the Rencpp repository...prior to the release of that project.  When that occurs, the destination repository will be:

    http://github.com/hostilefork/ren-garden

Please put any issues related to this project specifically (as opposed to Rencpp in general) in the issue database attached to that repository.  Bounties on those features may incentivize their implementation.  :-)
